#pragma once

#include "GLTexture.hpp"
#include "core/Stopwatch.hpp"
#include "core/Types.hpp"
#include "engine/IVideoEngine.hpp"
#include "utils/FpsTracker.hpp"
#include "utils/V4L2Enumerator.hpp"
#include <string>
#include <memory>

struct GLFWwindow;
struct ImFont;

namespace g2g {

class UIRenderer {
public:
    UIRenderer();
    ~UIRenderer();

    bool init(GLFWwindow* window);
    void shutdown();

    void render(
        GLFWwindow* window,
        Stopwatch& stopwatch,
        IVideoEngine* current_engine,
        EngineConfig& config,
        bool& engine_changed,
        bool& stream_start_requested,
        bool& stream_stop_requested,
        bool& vsync_enabled
    );

    // Update video texture with latest frame
    void updateVideoFrame(const VideoFrame& frame);

    // Handle keyboard shortcuts
    void handleKeyPress(int key, int action, Stopwatch& stopwatch);

    // Freeze management
    void toggleFreeze(const Stopwatch& stopwatch);
    bool isFrozen() const { return is_frozen_; }

    void requestSnapshot() { snapshot_requested_ = true; }

private:
    void renderHeader(const EngineStats& stats, const EngineConfig& config);
    void renderStopwatchPanel(Stopwatch& stopwatch);
    void renderVideoPanel(Stopwatch& stopwatch);
    void renderConfigPanel(
        EngineConfig& config,
        const EngineStats& stats,
        bool& engine_changed,
        bool& stream_start_requested,
        bool& stream_stop_requested,
        bool& vsync_enabled
    );

    GLFWwindow* window_{nullptr};
    GLTexture video_texture_;
    VideoFrame latest_frame_;
    VideoFrame frozen_frame_;

    bool is_frozen_{false};
    uint64_t frozen_stopwatch_us_{0};
    bool snapshot_requested_{false};
    std::string last_saved_path_;

    void refreshV4L2Devices(EngineConfig& config);
    void syncV4L2Selection(EngineConfig& config);

    std::vector<V4L2DeviceInfo> v4l2_devices_;
    int selected_device_idx_{0};
    int selected_profile_idx_{0};
    bool v4l2_initialized_{false};

    FpsTracker render_fps_tracker_;
    ImFont* regular_font_{nullptr};
    ImFont* large_font_{nullptr};
};

} // namespace g2g
