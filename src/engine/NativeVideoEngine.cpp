#include "NativeVideoEngine.hpp"
#include "core/Stopwatch.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace g2g {

NativeVideoEngine::NativeVideoEngine() {
}

NativeVideoEngine::~NativeVideoEngine() {
    stop();
}

bool NativeVideoEngine::initDecoder(AVCodecID codec_id) {
    cleanupDecoder();

    av_codec_ = avcodec_find_decoder(codec_id);
    if (!av_codec_) {
        std::cerr << "[NativeVideoEngine] Codec not found for id: " << codec_id << std::endl;
        return false;
    }

    av_ctx_ = avcodec_alloc_context3(av_codec_);
    if (!av_ctx_) {
        std::cerr << "[NativeVideoEngine] Failed to allocate codec context" << std::endl;
        return false;
    }

    // Zero-latency configuration
    av_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    av_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
    av_ctx_->thread_count = 1; // Single thread for strictly zero queue latency

    if (avcodec_open2(av_ctx_, av_codec_, nullptr) < 0) {
        std::cerr << "[NativeVideoEngine] Failed to open codec" << std::endl;
        cleanupDecoder();
        return false;
    }

    av_parser_ = av_parser_init(codec_id);
    av_frame_ = av_frame_alloc();
    av_packet_ = av_packet_alloc();

    return true;
}

void NativeVideoEngine::cleanupDecoder() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    if (av_frame_) {
        av_frame_free(&av_frame_);
        av_frame_ = nullptr;
    }
    if (av_packet_) {
        av_packet_free(&av_packet_);
        av_packet_ = nullptr;
    }
    if (av_parser_) {
        av_parser_close(av_parser_);
        av_parser_ = nullptr;
    }
    if (av_ctx_) {
        avcodec_free_context(&av_ctx_);
        av_ctx_ = nullptr;
    }
    av_codec_ = nullptr;
    sws_last_w_ = 0;
    sws_last_h_ = 0;
    sws_last_fmt_ = AV_PIX_FMT_NONE;
}

void NativeVideoEngine::decodeAndPublish(const uint8_t* data, size_t size) {
    if (!av_ctx_ || !data || size == 0) return;

    uint8_t* cur_data = const_cast<uint8_t*>(data);
    int cur_size = static_cast<int>(size);

    while (cur_size > 0) {
        uint8_t* pout = nullptr;
        int pout_len = 0;
        int parsed_len = cur_size;

        if (av_parser_) {
            parsed_len = av_parser_parse2(
                av_parser_, av_ctx_, &pout, &pout_len,
                cur_data, cur_size,
                AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0
            );
        } else {
            pout = cur_data;
            pout_len = cur_size;
        }

        cur_data += parsed_len;
        cur_size -= parsed_len;

        if (pout_len <= 0) continue;

        av_packet_->data = pout;
        av_packet_->size = pout_len;

        if (avcodec_send_packet(av_ctx_, av_packet_) < 0) {
            continue;
        }

        while (avcodec_receive_frame(av_ctx_, av_frame_) == 0) {
            int w = av_frame_->width;
            int h = av_frame_->height;
            auto fmt = static_cast<AVPixelFormat>(av_frame_->format);

            if (w <= 0 || h <= 0) continue;

            if (!sws_ctx_ || sws_last_w_ != w || sws_last_h_ != h || sws_last_fmt_ != fmt) {
                if (sws_ctx_) sws_freeContext(sws_ctx_);
                sws_ctx_ = sws_getContext(
                    w, h, fmt,
                    w, h, AV_PIX_FMT_RGBA,
                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
                );
                sws_last_w_ = w;
                sws_last_h_ = h;
                sws_last_fmt_ = fmt;
            }

            if (!sws_ctx_) continue;

            VideoFrame& write_buf = frame_queue_.getWriteBuffer();
            write_buf.width = w;
            write_buf.height = h;
            write_buf.timestamp_us = Stopwatch::getMonotonicRawUs();
            write_buf.frame_index = ++frame_counter_;

            size_t expected_size = static_cast<size_t>(w * h * 4);
            if (write_buf.data.size() != expected_size) {
                write_buf.data.resize(expected_size);
            }

            uint8_t* dst_data[4] = { write_buf.data.data(), nullptr, nullptr, nullptr };
            int dst_linesize[4] = { w * 4, 0, 0, 0 };

            sws_scale(sws_ctx_, av_frame_->data, av_frame_->linesize, 0, h, dst_data, dst_linesize);

            frame_queue_.publishFrame();

            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                capture_fps_tracker_.tick(write_buf.timestamp_us);
            }
            signal_lost_.store(false, std::memory_order_release);
        }
    }
}

void NativeVideoEngine::convertYUYVtoRGBA(const uint8_t* yuyv, uint8_t* rgba, int width, int height) {
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i += 2) {
        int yuyv_idx = i * 2;
        int y0 = yuyv[yuyv_idx + 0];
        int u  = yuyv[yuyv_idx + 1] - 128;
        int y1 = yuyv[yuyv_idx + 2];
        int v  = yuyv[yuyv_idx + 3] - 128;

        auto clamp = [](int val) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(val, 0, 255));
        };

        int r0 = y0 + ((1792 * v) >> 10);
        int g0 = y0 - ((360 * u + 728 * v) >> 10);
        int b0 = y0 + ((2264 * u) >> 10);

        int r1 = y1 + ((1792 * v) >> 10);
        int g1 = y1 - ((360 * u + 728 * v) >> 10);
        int b1 = y1 + ((2264 * u) >> 10);

        int rgba_idx = i * 4;
        rgba[rgba_idx + 0] = clamp(r0);
        rgba[rgba_idx + 1] = clamp(g0);
        rgba[rgba_idx + 2] = clamp(b0);
        rgba[rgba_idx + 3] = 255;

        rgba[rgba_idx + 4] = clamp(r1);
        rgba[rgba_idx + 5] = clamp(g1);
        rgba[rgba_idx + 6] = clamp(b1);
        rgba[rgba_idx + 7] = 255;
    }
}

void NativeVideoEngine::uvcWorkerLoop(std::string device, int width, int height, int fps) {
    (void)fps;
    int fd = open(device.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        std::cerr << "[NativeVideoEngine] Failed to open V4L2 device: " << device << std::endl;
        signal_lost_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        status_msg_ = "Cannot open V4L2 device: " + device;
        return;
    }

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cerr << "[NativeVideoEngine] Device does not support video capture" << std::endl;
        close(fd);
        signal_lost_.store(true, std::memory_order_release);
        return;
    }

    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    bool is_mjpeg = false;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        // Try MJPEG fallback
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            std::cerr << "[NativeVideoEngine] Failed to set V4L2 format" << std::endl;
            close(fd);
            signal_lost_.store(true, std::memory_order_release);
            return;
        }
        is_mjpeg = true;
    }

    int actual_w = fmt.fmt.pix.width;
    int actual_h = fmt.fmt.pix.height;

    if (is_mjpeg) {
        initDecoder(AV_CODEC_ID_MJPEG);
    }

    // Request 2 buffers for minimal queue delay
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
        std::cerr << "[NativeVideoEngine] VIDIOC_REQBUFS failed" << std::endl;
        close(fd);
        signal_lost_.store(true, std::memory_order_release);
        return;
    }

    struct BufferMap {
        void* start;
        size_t length;
    };
    std::vector<BufferMap> buffers(req.count);

    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) break;

        buffers[i].length = buf.length;
        buffers[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            std::cerr << "[NativeVideoEngine] mmap failed" << std::endl;
            close(fd);
            signal_lost_.store(true, std::memory_order_release);
            return;
        }

        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        std::cerr << "[NativeVideoEngine] VIDIOC_STREAMON failed" << std::endl;
        close(fd);
        signal_lost_.store(true, std::memory_order_release);
        return;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (!stop_thread_.load(std::memory_order_relaxed)) {
        int ret = poll(&pfd, 1, 100);
        if (ret < 0) break;
        if (ret == 0) continue; // Timeout

        if (pfd.revents & POLLIN) {
            struct v4l2_buffer buf;
            std::memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(fd, VIDIOC_DQBUF, &buf) == 0) {
                uint8_t* raw_ptr = static_cast<uint8_t*>(buffers[buf.index].start);
                if (is_mjpeg) {
                    decodeAndPublish(raw_ptr, buf.bytesused);
                } else {
                    VideoFrame& write_buf = frame_queue_.getWriteBuffer();
                    write_buf.width = actual_w;
                    write_buf.height = actual_h;
                    write_buf.timestamp_us = Stopwatch::getMonotonicRawUs();
                    write_buf.frame_index = ++frame_counter_;

                    size_t expected_size = static_cast<size_t>(actual_w * actual_h * 4);
                    if (write_buf.data.size() != expected_size) {
                        write_buf.data.resize(expected_size);
                    }

                    convertYUYVtoRGBA(raw_ptr, write_buf.data.data(), actual_w, actual_h);
                    frame_queue_.publishFrame();

                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        capture_fps_tracker_.tick(write_buf.timestamp_us);
                    }
                    signal_lost_.store(false, std::memory_order_release);
                }

                ioctl(fd, VIDIOC_QBUF, &buf);
            }
        }
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (auto& b : buffers) {
        if (b.start && b.start != MAP_FAILED) {
            munmap(b.start, b.length);
        }
    }
    close(fd);
    cleanupDecoder();
}

void NativeVideoEngine::udpWorkerLoop(int port, std::string payload) {
    (void)payload;
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        std::cerr << "[NativeVideoEngine] Socket creation failed" << std::endl;
        signal_lost_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        status_msg_ = "Socket creation failed";
        return;
    }

    int reuse = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Increase socket receive buffer size
    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[NativeVideoEngine] UDP Bind failed on port " << port << std::endl;
        close(sock_fd);
        signal_lost_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        status_msg_ = "Bind failed on port " + std::to_string(port);
        return;
    }

    initDecoder(AV_CODEC_ID_H264);

    struct pollfd pfd;
    pfd.fd = sock_fd;
    pfd.events = POLLIN;

    std::vector<uint8_t> rx_buf(65536);
    fua_buffer_.clear();
    fua_started_ = false;

    while (!stop_thread_.load(std::memory_order_relaxed)) {
        int ret = poll(&pfd, 1, 100);
        if (ret < 0) break;
        if (ret == 0) continue; // Timeout

        if (pfd.revents & POLLIN) {
            ssize_t len = recvfrom(sock_fd, rx_buf.data(), rx_buf.size(), 0, nullptr, nullptr);
            if (len <= 12) continue; // Too short for RTP (12 bytes min header)

            // Parse RTP header
            const uint8_t* payload_ptr = rx_buf.data() + 12;
            size_t payload_len = len - 12;

            if (payload_len < 1) continue;

            uint8_t nal_type = payload_ptr[0] & 0x1F;

            // RFC 6184 H.264 RTP Packetization
            if (nal_type >= 1 && nal_type <= 23) {
                // Single NAL unit packet
                std::vector<uint8_t> nalu;
                nalu.reserve(payload_len + 4);
                nalu.push_back(0x00);
                nalu.push_back(0x00);
                nalu.push_back(0x00);
                nalu.push_back(0x01);
                nalu.insert(nalu.end(), payload_ptr, payload_ptr + payload_len);

                decodeAndPublish(nalu.data(), nalu.size());
            } else if (nal_type == 28) {
                // FU-A Fragmented NAL unit
                if (payload_len < 2) continue;
                uint8_t fu_indicator = payload_ptr[0];
                uint8_t fu_header = payload_ptr[1];
                bool is_start = (fu_header & 0x80) != 0;
                bool is_end = (fu_header & 0x40) != 0;
                uint8_t orig_nal_type = fu_header & 0x1F;
                uint8_t reconstructed_nal = (fu_indicator & 0xE0) | orig_nal_type;

                const uint8_t* fu_data = payload_ptr + 2;
                size_t fu_data_len = payload_len - 2;

                if (is_start) {
                    fua_buffer_.clear();
                    fua_buffer_.push_back(0x00);
                    fua_buffer_.push_back(0x00);
                    fua_buffer_.push_back(0x00);
                    fua_buffer_.push_back(0x01);
                    fua_buffer_.push_back(reconstructed_nal);
                    fua_buffer_.insert(fua_buffer_.end(), fu_data, fu_data + fu_data_len);
                    fua_started_ = true;
                } else if (fua_started_) {
                    fua_buffer_.insert(fua_buffer_.end(), fu_data, fu_data + fu_data_len);
                    if (is_end) {
                        decodeAndPublish(fua_buffer_.data(), fua_buffer_.size());
                        fua_buffer_.clear();
                        fua_started_ = false;
                    }
                }
            } else {
                // STAP-A or other: Feed raw to decoder parser
                decodeAndPublish(payload_ptr, payload_len);
            }
        }
    }

    close(sock_fd);
    cleanupDecoder();
}

bool NativeVideoEngine::start(const EngineConfig& config) {
    stop();

    stop_thread_.store(false, std::memory_order_release);
    signal_lost_.store(false, std::memory_order_release);

    if (config.source_type == SourceType::V4L2) {
        status_msg_ = "Running (Native V4L2 mmap)";
        worker_thread_ = std::thread(&NativeVideoEngine::uvcWorkerLoop, this, config.v4l2_device, config.v4l2_width, config.v4l2_height, config.v4l2_fps);
    } else {
        status_msg_ = "Running (Native UDP Socket + avcodec)";
        worker_thread_ = std::thread(&NativeVideoEngine::udpWorkerLoop, this, config.udp_port, config.udp_payload);
    }

    is_running_.store(true, std::memory_order_release);
    return true;
}

void NativeVideoEngine::stop() {
    is_running_.store(false, std::memory_order_release);
    stop_thread_.store(true, std::memory_order_release);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    frame_queue_.resetStats();
    capture_fps_tracker_.reset();
    status_msg_ = "Stopped";
}

bool NativeVideoEngine::isRunning() const {
    return is_running_.load(std::memory_order_acquire);
}

bool NativeVideoEngine::getLatestFrame(VideoFrame& out_frame) {
    return frame_queue_.acquireLatestFrame(out_frame);
}

EngineStats NativeVideoEngine::getStats() const {
    EngineStats stats;
    stats.is_running = is_running_.load(std::memory_order_relaxed);
    stats.is_signal_lost = signal_lost_.load(std::memory_order_relaxed);
    stats.dropped_frames = frame_queue_.getDroppedCount();
    stats.total_frames = frame_queue_.getTotalPushed();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats.capture_fps = capture_fps_tracker_.getFps();
        stats.status_message = status_msg_;
    }

    return stats;
}

} // namespace g2g
