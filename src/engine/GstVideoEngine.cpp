#include "GstVideoEngine.hpp"
#include "core/Stopwatch.hpp"
#include <gst/video/video.h>
#include <iostream>
#include <sstream>
#include <cstring>

namespace g2g {

static std::once_flag g_gst_init_flag;
static void initGstreamerIfNeeded() {
    std::call_once(g_gst_init_flag, []() {
        if (!gst_is_initialized()) {
            gst_init(nullptr, nullptr);
        }
    });
}

GstVideoEngine::GstVideoEngine() {
    initGstreamerIfNeeded();
}

GstVideoEngine::~GstVideoEngine() {
    stop();
}

std::string GstVideoEngine::buildPipelineString(const EngineConfig& config) {
    std::ostringstream ss;
    if (config.source_type == SourceType::V4L2) {
        ss << "v4l2src device=" << config.v4l2_device << " io-mode=mmap "
           << "! videoconvert "
           << "! video/x-raw,format=RGBA "
           << "! appsink name=sink sync=false drop=true max-buffers=1";
    } else if (config.source_type == SourceType::UDP_RTP) {
        ss << "udpsrc port=" << config.udp_port << " caps=\"application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264\" "
           << "! rtpjitterbuffer latency=0 drop-on-latency=true "
           << "! rtph264depay "
           << "! avdec_h264 "
           << "! videoconvert "
           << "! video/x-raw,format=RGBA "
           << "! appsink name=sink sync=false drop=true max-buffers=1";
    } else if (config.source_type == SourceType::RTSP) {
        ss << "rtspsrc location=" << config.rtsp_url << " latency=0 buffer-mode=none drop-on-latency=true "
           << "! rtph264depay "
           << "! avdec_h264 "
           << "! videoconvert "
           << "! video/x-raw,format=RGBA "
           << "! appsink name=sink sync=false drop=true max-buffers=1";
    }
    return ss.str();
}

GstFlowReturn GstVideoEngine::onNewSample(GstAppSink* sink, gpointer user_data) {
    auto* self = static_cast<GstVideoEngine*>(user_data);
    if (!self || !self->is_running_.load(std::memory_order_relaxed)) {
        return GST_FLOW_OK;
    }

    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_OK;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);
    int width = 0;
    int height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (buffer && width > 0 && height > 0) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            VideoFrame& write_buf = self->frame_queue_.getWriteBuffer();
            write_buf.width = width;
            write_buf.height = height;
            write_buf.timestamp_us = Stopwatch::getMonotonicRawUs();
            write_buf.frame_index = ++self->frame_counter_;

            size_t expected_size = static_cast<size_t>(width * height * 4);
            if (write_buf.data.size() != expected_size) {
                write_buf.data.resize(expected_size);
            }

            size_t copy_size = std::min(expected_size, static_cast<size_t>(map.size));
            std::memcpy(write_buf.data.data(), map.data, copy_size);

            self->frame_queue_.publishFrame();

            {
                std::lock_guard<std::mutex> lock(self->stats_mutex_);
                self->capture_fps_tracker_.tick(write_buf.timestamp_us);
            }

            self->signal_lost_.store(false, std::memory_order_release);
            gst_buffer_unmap(buffer, &map);
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void GstVideoEngine::busWatchThreadFunc() {
    if (!pipeline_) return;
    GstBus* bus = gst_element_get_bus(pipeline_);
    if (!bus) return;

    while (!stop_bus_thread_.load(std::memory_order_relaxed)) {
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, 
            100 * GST_MSECOND, 
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_WARNING)
        );

        if (!msg) continue;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr;
                gchar* dbg = nullptr;
                gst_message_parse_error(msg, &err, &dbg);
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    status_msg_ = "GStreamer Error: " + (err ? std::string(err->message) : "Unknown");
                }
                std::cerr << "[GstVideoEngine] Error: " << (err ? err->message : "") << " (" << (dbg ? dbg : "") << ")" << std::endl;
                if (err) g_error_free(err);
                if (dbg) g_free(dbg);
                signal_lost_.store(true, std::memory_order_release);
                break;
            }
            case GST_MESSAGE_WARNING: {
                GError* err = nullptr;
                gchar* dbg = nullptr;
                gst_message_parse_warning(msg, &err, &dbg);
                std::cout << "[GstVideoEngine] Warning: " << (err ? err->message : "") << std::endl;
                if (err) g_error_free(err);
                if (dbg) g_free(dbg);
                break;
            }
            case GST_MESSAGE_EOS: {
                std::cout << "[GstVideoEngine] End of stream reached." << std::endl;
                signal_lost_.store(true, std::memory_order_release);
                break;
            }
            default:
                break;
        }

        gst_message_unref(msg);
    }

    gst_object_unref(bus);
}

bool GstVideoEngine::start(const EngineConfig& config) {
    stop();

    std::string pipe_str = buildPipelineString(config);
    std::cout << "[GstVideoEngine] Launching pipeline: " << pipe_str << std::endl;

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipe_str.c_str(), &error);
    if (!pipeline_ || error) {
        std::cerr << "[GstVideoEngine] Failed to create pipeline: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        status_msg_ = "Pipeline build failed";
        return false;
    }

    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (!appsink_) {
        std::cerr << "[GstVideoEngine] Failed to get appsink element" << std::endl;
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        status_msg_ = "appsink not found";
        return false;
    }

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = onNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink_), &callbacks, this, nullptr);

    stop_bus_thread_.store(false, std::memory_order_release);
    bus_thread_ = std::thread(&GstVideoEngine::busWatchThreadFunc, this);

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[GstVideoEngine] Failed to set pipeline to PLAYING" << std::endl;
        stop();
        status_msg_ = "State change failure";
        return false;
    }

    is_running_.store(true, std::memory_order_release);
    signal_lost_.store(false, std::memory_order_release);
    status_msg_ = "Running (GStreamer)";
    return true;
}

void GstVideoEngine::stop() {
    is_running_.store(false, std::memory_order_release);
    stop_bus_thread_.store(true, std::memory_order_release);

    if (bus_thread_.joinable()) {
        bus_thread_.join();
    }

    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (appsink_) {
            gst_object_unref(appsink_);
            appsink_ = nullptr;
        }
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }

    frame_queue_.resetStats();
    capture_fps_tracker_.reset();
    status_msg_ = "Stopped";
}

bool GstVideoEngine::isRunning() const {
    return is_running_.load(std::memory_order_acquire);
}

bool GstVideoEngine::getLatestFrame(VideoFrame& out_frame) {
    return frame_queue_.acquireLatestFrame(out_frame);
}

EngineStats GstVideoEngine::getStats() const {
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
