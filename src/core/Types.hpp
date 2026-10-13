#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace g2g {

enum class EngineType {
    GSTREAMER = 0,
    NATIVE = 1
};

enum class SourceType {
    V4L2 = 0,
    UDP_RTP = 1,
    RTSP = 2
};

struct EngineConfig {
    EngineType engine_type = EngineType::GSTREAMER;
    SourceType source_type = SourceType::UDP_RTP;
    
    // V4L2 settings
    std::string v4l2_device = "/dev/video0";
    int v4l2_width = 1920;
    int v4l2_height = 1080;
    int v4l2_fps = 60;
    
    // UDP settings
    int udp_port = 5004;
    std::string udp_payload = "H264";
    
    // RTSP settings
    std::string rtsp_url = "rtsp://127.0.0.1:8554/live";
};

struct VideoFrame {
    std::vector<uint8_t> data; // RGBA byte data
    int width = 0;
    int height = 0;
    uint64_t timestamp_us = 0; // Monotonic timestamp in microseconds
    uint64_t frame_index = 0;
};

struct EngineStats {
    double capture_fps = 0.0;
    double render_fps = 0.0;
    uint64_t total_frames = 0;
    uint64_t dropped_frames = 0;
    bool is_running = false;
    bool is_signal_lost = false;
    std::string status_message = "Idle";
};

} // namespace g2g
