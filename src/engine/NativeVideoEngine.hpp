#pragma once

#include "IVideoEngine.hpp"
#include "core/FrameQueue.hpp"
#include "utils/FpsTracker.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

namespace g2g {

class NativeVideoEngine : public IVideoEngine {
public:
    NativeVideoEngine();
    ~NativeVideoEngine() override;

    bool start(const EngineConfig& config) override;
    void stop() override;
    bool isRunning() const override;
    bool getLatestFrame(VideoFrame& out_frame) override;
    EngineStats getStats() const override;

private:
    void uvcWorkerLoop(std::string device, int width, int height, int fps, std::string format);
    void udpWorkerLoop(int port, std::string payload);
    void rtspWorkerLoop(std::string rtsp_url);

    bool initDecoder(AVCodecID codec_id);
    void cleanupDecoder();
    void decodeAndPublish(const uint8_t* data, size_t size);
    void publishDecodedFrame(AVFrame* frame);

    // Color conversion helpers
    static void convertYUYVtoRGBA(const uint8_t* yuyv, uint8_t* rgba, int width, int height);

    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_thread_{false};
    std::atomic<bool> signal_lost_{false};
    std::string status_msg_{"Stopped"};

    std::thread worker_thread_;

    FrameQueue frame_queue_;
    mutable FpsTracker capture_fps_tracker_;
    mutable std::mutex stats_mutex_;
    uint64_t frame_counter_{0};

    // FFmpeg decoder state
    const AVCodec* av_codec_{nullptr};
    AVCodecContext* av_ctx_{nullptr};
    AVCodecParserContext* av_parser_{nullptr};
    AVFrame* av_frame_{nullptr};
    AVPacket* av_packet_{nullptr};
    SwsContext* sws_ctx_{nullptr};
    int sws_last_w_{0};
    int sws_last_h_{0};
    AVPixelFormat sws_last_fmt_{AV_PIX_FMT_NONE};

    // FU-A assembly buffer for RTP H.264
    std::vector<uint8_t> fua_buffer_;
    bool fua_started_{false};
};

} // namespace g2g
