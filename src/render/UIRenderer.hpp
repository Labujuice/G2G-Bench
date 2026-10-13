#pragma once

#include "GLTexture.hpp"
#include "core/Stopwatch.hpp"
#include "core/Types.hpp"
#include "engine/IVideoEngine.hpp"
#include "utils/FpsTracker.hpp"
#include <string>
#include <memory>

struct GLFWwindow;

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

private:
    void renderHeader(const EngineStats& stats, const EngineConfig& config);
    void renderStopwatchPanel(Stopwatch& stopwatch);
    void renderVideoPanel();
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
    std::string last_saved_path_;

    FpsTracker render_fps_tracker_;
};

} // namespace g2g
