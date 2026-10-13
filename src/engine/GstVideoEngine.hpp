#pragma once

#include "IVideoEngine.hpp"
#include "core/FrameQueue.hpp"
#include "utils/FpsTracker.hpp"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <atomic>
#include <thread>
#include <mutex>

namespace g2g {

class GstVideoEngine : public IVideoEngine {
public:
    GstVideoEngine();
    ~GstVideoEngine() override;

    bool start(const EngineConfig& config) override;
    void stop() override;
    bool isRunning() const override;
    bool getLatestFrame(VideoFrame& out_frame) override;
    EngineStats getStats() const override;

private:
    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer user_data);
    void busWatchThreadFunc();
    std::string buildPipelineString(const EngineConfig& config);

    std::atomic<bool> is_running_{false};
    std::atomic<bool> signal_lost_{false};
    std::string status_msg_{"Stopped"};

    GstElement* pipeline_{nullptr};
    GstElement* appsink_{nullptr};

    std::thread bus_thread_;
    std::atomic<bool> stop_bus_thread_{false};

    FrameQueue frame_queue_;
    mutable FpsTracker capture_fps_tracker_;
    mutable std::mutex stats_mutex_;
    uint64_t frame_counter_{0};
};

} // namespace g2g
