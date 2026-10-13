#include "UIRenderer.hpp"
#include "utils/ImageDumper.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace g2g {

UIRenderer::UIRenderer() = default;

UIRenderer::~UIRenderer() {
    shutdown();
}

bool UIRenderer::init(GLFWwindow* window) {
    window_ = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load clean TrueType font if available, otherwise use ImGui default
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    const char* mono_bold_path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf";

    if (FILE* f = fopen(font_path, "rb")) {
        fclose(f);
        regular_font_ = io.Fonts->AddFontFromFileTTF(font_path, 16.0f);
    }
    if (FILE* f = fopen(mono_bold_path, "rb")) {
        fclose(f);
        large_font_ = io.Fonts->AddFontFromFileTTF(mono_bold_path, 80.0f);
    } else if (regular_font_) {
        large_font_ = io.Fonts->AddFontFromFileTTF(font_path, 80.0f);
    }

    if (!regular_font_) {
        regular_font_ = io.Fonts->AddFontDefault();
    }

    // Modern clean dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        std::cerr << "[UIRenderer] Failed to init ImGui GLFW backend" << std::endl;
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        std::cerr << "[UIRenderer] Failed to init ImGui OpenGL3 backend" << std::endl;
        return false;
    }

    video_texture_.init();
    return true;
}

void UIRenderer::shutdown() {
    video_texture_.destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    window_ = nullptr;
}

void UIRenderer::updateVideoFrame(const VideoFrame& frame) {
    latest_frame_ = frame;
    if (!is_frozen_ && frame.width > 0 && frame.height > 0 && !frame.data.empty()) {
        video_texture_.upload(frame.data.data(), frame.width, frame.height);
    }
}

void UIRenderer::toggleFreeze(const Stopwatch& stopwatch) {
    is_frozen_ = !is_frozen_;
    if (is_frozen_) {
        frozen_stopwatch_us_ = stopwatch.getElapsedMicroseconds();
        frozen_frame_ = latest_frame_;
        if (frozen_frame_.width > 0 && frozen_frame_.height > 0) {
            video_texture_.upload(frozen_frame_.data.data(), frozen_frame_.width, frozen_frame_.height);
        }
    } else {
        if (latest_frame_.width > 0 && latest_frame_.height > 0) {
            video_texture_.upload(latest_frame_.data.data(), latest_frame_.width, latest_frame_.height);
        }
    }
}

void UIRenderer::handleKeyPress(int key, int action, Stopwatch& stopwatch) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_SPACE) {
        stopwatch.toggle();
    } else if (key == GLFW_KEY_R) {
        stopwatch.reset();
        if (is_frozen_) {
            frozen_stopwatch_us_ = 0;
        }
    } else if (key == GLFW_KEY_F) {
        toggleFreeze(stopwatch);
    } else if (key == GLFW_KEY_S) {
        snapshot_requested_ = true;
    }
}

void UIRenderer::renderHeader(const EngineStats& stats, const EngineConfig& config) {
    std::string engine_str = (config.engine_type == EngineType::GSTREAMER) ? "GStreamer Pipeline" : "Pure C++ Native Direct IO";
    std::string status_badge = stats.is_running ? (stats.is_signal_lost ? "SIGNAL LOST" : "RUNNING") : "STOPPED";

    ImVec4 status_color = stats.is_running ? (stats.is_signal_lost ? ImVec4(1.0f, 0.4f, 0.0f, 1.0f) : ImVec4(0.2f, 0.9f, 0.2f, 1.0f)) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "G2G-Bench v1.0 [Glass-to-Glass Benchmark]");
    ImGui::SameLine();
    ImGui::Text("| Engine: [%s] |", engine_str.c_str());
    ImGui::SameLine();
    ImGui::TextColored(status_color, "[Status: %s]", status_badge.c_str());

    if (is_frozen_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "[** FROZEN COMPARISON ACTIVE **]");
    }

    ImGui::Separator();
}

void UIRenderer::renderStopwatchPanel(Stopwatch& stopwatch) {
    ImGui::BeginChild("StopwatchPanel", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ Reference Stopwatch ]");
    ImGui::Separator();

    uint64_t current_us = is_frozen_ ? frozen_stopwatch_us_ : stopwatch.getElapsedMicroseconds();
    std::string time_str = Stopwatch::formatTimeMs(current_us);

    float window_width = ImGui::GetWindowSize().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Center content vertically
    float content_h = 240.0f;
    float top_dummy = (avail_h - content_h) * 0.32f;
    if (top_dummy > 10.0f) {
        ImGui::Dummy(ImVec2(0.0f, top_dummy));
    }

    // Centered prominent huge time display
    if (large_font_) {
        ImGui::PushFont(large_font_);
    } else {
        ImGui::SetWindowFontScale(4.8f);
    }
    float text_width = ImGui::CalcTextSize(time_str.c_str()).x;
    ImGui::SetCursorPosX((window_width - text_width) * 0.5f);

    if (stopwatch.isRunning() && !is_frozen_) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", time_str.c_str());
    } else if (is_frozen_) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.15f, 1.0f), "%s", time_str.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", time_str.c_str());
    }

    if (large_font_) {
        ImGui::PopFont();
    } else {
        ImGui::SetWindowFontScale(1.0f);
    }

    // Timestamp & Precision info
    std::string sub_us_str = "CLOCK_MONOTONIC_RAW: " + std::to_string(current_us) + " us  (1ms Precision)";
    float sub_width = ImGui::CalcTextSize(sub_us_str.c_str()).x;
    ImGui::SetCursorPosX((window_width - sub_width) * 0.5f);
    ImGui::TextDisabled("%s", sub_us_str.c_str());

    ImGui::Dummy(ImVec2(0.0f, 28.0f));

    // Stopwatch Controls
    float btn_w = 140.0f;
    float btn_h = 42.0f;
    float total_btn_w = btn_w * 2 + 16.0f;
    ImGui::SetCursorPosX((window_width - total_btn_w) * 0.5f);

    if (stopwatch.isRunning()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.4f, 0.1f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.5f, 0.15f, 1.0f));
        if (ImGui::Button("Pause", ImVec2(btn_w, btn_h))) {
            stopwatch.toggle();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.25f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.8f, 0.35f, 1.0f));
        if (ImGui::Button("Start", ImVec2(btn_w, btn_h))) {
            stopwatch.toggle();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Button("Reset", ImVec2(btn_w, btn_h))) {
        stopwatch.reset();
        if (is_frozen_) frozen_stopwatch_us_ = 0;
    }

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    const char* hint_text = "Shortcuts: [Space] Start / Pause  |  [R] Reset Stopwatch";
    float hint_w = ImGui::CalcTextSize(hint_text).x;
    ImGui::SetCursorPosX((window_width - hint_w) * 0.5f);
    ImGui::TextDisabled("%s", hint_text);

    ImGui::EndChild();
}

void UIRenderer::renderVideoPanel(Stopwatch& stopwatch) {
    ImGui::BeginChild("VideoPanel", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ Video Stream Preview ]");
    if (video_texture_.isValid()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%dx%d)", video_texture_.getWidth(), video_texture_.getHeight());
    }
    ImGui::Separator();

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y - 55.0f;

    if (video_texture_.isValid()) {
        int tex_w = video_texture_.getWidth();
        int tex_h = video_texture_.getHeight();

        float aspect = static_cast<float>(tex_w) / static_cast<float>(tex_h);
        float render_w = avail_w;
        float render_h = avail_w / aspect;

        if (render_h > avail_h) {
            render_h = avail_h;
            render_w = avail_h * aspect;
        }

        float offset_x = (avail_w - render_w) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

        ImGui::Image(
            reinterpret_cast<void*>(static_cast<intptr_t>(video_texture_.getTextureId())),
            ImVec2(render_w, render_h)
        );
    } else {
        // Placeholder
        ImGui::Dummy(ImVec2(0.0f, avail_h * 0.35f));
        const char* msg = "Waiting for Video Stream...";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((avail_w - tw) * 0.5f);
        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "%s", msg);
        ImGui::Dummy(ImVec2(0.0f, avail_h * 0.35f));
    }

    ImGui::Separator();

    // Action Controls (Freeze & Snapshot)
    float btn_h = 36.0f;
    if (is_frozen_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.7f, 0.1f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
        if (ImGui::Button("Resume Live [F]", ImVec2(160.0f, btn_h))) {
            toggleFreeze(stopwatch);
        }
        ImGui::PopStyleColor(3);
    } else {
        if (ImGui::Button("Freeze Compare [F]", ImVec2(160.0f, btn_h))) {
            toggleFreeze(stopwatch);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Snapshot (S)", ImVec2(160.0f, btn_h))) {
        snapshot_requested_ = true;
    }

    if (!last_saved_path_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.95f, 0.4f, 1.0f), "[Saved: %s]", last_saved_path_.c_str());
    }

    ImGui::EndChild();
}

void UIRenderer::refreshV4L2Devices(EngineConfig& config) {
    v4l2_devices_ = V4L2Enumerator::enumerateDevices();
    selected_device_idx_ = 0;
    for (size_t i = 0; i < v4l2_devices_.size(); ++i) {
        if (v4l2_devices_[i].path == config.v4l2_device) {
            selected_device_idx_ = static_cast<int>(i);
            break;
        }
    }
    syncV4L2Selection(config);
    v4l2_initialized_ = true;
}

void UIRenderer::syncV4L2Selection(EngineConfig& config) {
    if (v4l2_devices_.empty()) return;
    if (selected_device_idx_ < 0 || selected_device_idx_ >= static_cast<int>(v4l2_devices_.size())) {
        selected_device_idx_ = 0;
    }

    const auto& dev = v4l2_devices_[selected_device_idx_];
    config.v4l2_device = dev.path;

    if (dev.profiles.empty()) return;

    // Find best match for current config in profiles
    selected_profile_idx_ = 0;
    for (size_t i = 0; i < dev.profiles.size(); ++i) {
        const auto& prof = dev.profiles[i];
        if (prof.format == config.v4l2_format &&
            prof.width == config.v4l2_width &&
            prof.height == config.v4l2_height &&
            prof.fps == config.v4l2_fps) {
            selected_profile_idx_ = static_cast<int>(i);
            break;
        }
    }

    const auto& chosen = dev.profiles[selected_profile_idx_];
    config.v4l2_format = chosen.format;
    config.v4l2_width = chosen.width;
    config.v4l2_height = chosen.height;
    config.v4l2_fps = chosen.fps;
}

void UIRenderer::renderConfigPanel(
    EngineConfig& config,
    const EngineStats& stats,
    bool& engine_changed,
    bool& stream_start_requested,
    bool& stream_stop_requested,
    bool& vsync_enabled
) {
    if (!v4l2_initialized_) {
        refreshV4L2Devices(config);
    }

    ImGui::BeginChild("ConfigPanel", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Configuration & Diagnostics");
    ImGui::Separator();

    // Source selection
    ImGui::Text("Input Source:");
    ImGui::SameLine();
    int src_type = static_cast<int>(config.source_type);
    if (ImGui::RadioButton("V4L2 / UVC Camera", &src_type, 0)) {
        config.source_type = SourceType::V4L2;
        engine_changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("UDP-RTP Stream", &src_type, 1)) {
        config.source_type = SourceType::UDP_RTP;
        engine_changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("RTSP Stream", &src_type, 2)) {
        config.source_type = SourceType::RTSP;
        engine_changed = true;
    }

    // Source details
    if (config.source_type == SourceType::V4L2) {
        if (v4l2_devices_.empty()) {
            refreshV4L2Devices(config);
        }

        const auto& cur_dev = v4l2_devices_[selected_device_idx_];

        // 1. Device Combo
        std::string cur_dev_label = cur_dev.path + " (" + cur_dev.card_name + ")";
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("Device", cur_dev_label.c_str())) {
            for (size_t i = 0; i < v4l2_devices_.size(); ++i) {
                bool is_sel = (selected_device_idx_ == static_cast<int>(i));
                std::string item_label = v4l2_devices_[i].path + " (" + v4l2_devices_[i].card_name + ")";
                if (ImGui::Selectable(item_label.c_str(), is_sel)) {
                    selected_device_idx_ = static_cast<int>(i);
                    syncV4L2Selection(config);
                    engine_changed = true;
                }
                if (is_sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Rescan")) {
            refreshV4L2Devices(config);
            engine_changed = true;
        }

        // 2. Single Unified Profile Combo (Format + Resolution + FPS grouped together)
        ImGui::SameLine();
        std::string cur_profile_label = "No profiles available";
        if (!cur_dev.profiles.empty()) {
            if (selected_profile_idx_ < 0 || selected_profile_idx_ >= static_cast<int>(cur_dev.profiles.size())) {
                selected_profile_idx_ = 0;
            }
            cur_profile_label = cur_dev.profiles[selected_profile_idx_].label;
        }

        ImGui::SetNextItemWidth(340.0f);
        if (ImGui::BeginCombo("Profile (Format/Res/FPS)", cur_profile_label.c_str())) {
            for (size_t i = 0; i < cur_dev.profiles.size(); ++i) {
                bool is_sel = (selected_profile_idx_ == static_cast<int>(i));
                if (ImGui::Selectable(cur_dev.profiles[i].label.c_str(), is_sel)) {
                    selected_profile_idx_ = static_cast<int>(i);
                    const auto& chosen = cur_dev.profiles[selected_profile_idx_];
                    config.v4l2_format = chosen.format;
                    config.v4l2_width = chosen.width;
                    config.v4l2_height = chosen.height;
                    config.v4l2_fps = chosen.fps;
                    engine_changed = true;
                }
                if (is_sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else if (config.source_type == SourceType::UDP_RTP) {
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Port", &config.udp_port, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
            engine_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply##udp")) {
            engine_changed = true;
        }
        ImGui::SameLine();
        ImGui::Text("Payload: H.264 (RFC 6184)");
    } else if (config.source_type == SourceType::RTSP) {
        if (!rtsp_buf_initialized_) {
            std::strncpy(rtsp_url_buf_, config.rtsp_url.c_str(), sizeof(rtsp_url_buf_) - 1);
            rtsp_buf_initialized_ = true;
        }

        ImGui::SetNextItemWidth(380.0f);
        if (ImGui::InputText("RTSP URL", rtsp_url_buf_, sizeof(rtsp_url_buf_))) {
            config.rtsp_url = rtsp_url_buf_;
        }

        bool enter_pressed = ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));

        ImGui::SameLine();
        bool apply_clicked = ImGui::Button("Connect / Apply");
        if (enter_pressed || apply_clicked) {
            config.rtsp_url = rtsp_url_buf_;
            engine_changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Press Enter or Apply to update)");
    }

    ImGui::Separator();

    // Engine Selection
    ImGui::Text("Stream Engine:");
    ImGui::SameLine();
    int eng_type = static_cast<int>(config.engine_type);
    if (ImGui::RadioButton("GStreamer Pipeline (Engine A)", &eng_type, 0)) {
        config.engine_type = EngineType::GSTREAMER;
        engine_changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Pure C++ Native Direct IO (Engine B)", &eng_type, 1)) {
        config.engine_type = EngineType::NATIVE;
        engine_changed = true;
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20.0f, 0.0f));
    ImGui::SameLine();

    if (!stats.is_running) {
        if (ImGui::Button("Start Stream", ImVec2(120.0f, 26.0f))) {
            stream_start_requested = true;
        }
    } else {
        if (ImGui::Button("Stop Stream", ImVec2(120.0f, 26.0f))) {
            stream_stop_requested = true;
        }
    }

    ImGui::SameLine();
    bool vsync_val = vsync_enabled;
    if (ImGui::Checkbox("Enable VSync", &vsync_val)) {
        vsync_enabled = vsync_val;
        glfwSwapInterval(vsync_enabled ? 1 : 0);
    }

    ImGui::Separator();

    // Diagnostics Bar
    double render_fps = render_fps_tracker_.getFps();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "Real-Time Diagnostics:");
    ImGui::SameLine();
    ImGui::Text("Capture FPS: %.1f |", stats.capture_fps);
    ImGui::SameLine();
    ImGui::Text("Render FPS: %.1f |", render_fps);
    ImGui::SameLine();
    ImGui::Text("Dropped Frames: %lu |", stats.dropped_frames);
    ImGui::SameLine();
    ImGui::Text("Total Received: %lu |", stats.total_frames);
    ImGui::SameLine();
    ImGui::Text("Sync: None (Zero-Buffer)");

    ImGui::EndChild();
}

void UIRenderer::render(
    GLFWwindow* window,
    Stopwatch& stopwatch,
    IVideoEngine* current_engine,
    EngineConfig& config,
    bool& engine_changed,
    bool& stream_start_requested,
    bool& stream_stop_requested,
    bool& vsync_enabled
) {
    render_fps_tracker_.tick(Stopwatch::getMonotonicRawUs());

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    EngineStats stats = current_engine ? current_engine->getStats() : EngineStats{};

    // Main full window canvas
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), static_cast<float>(display_h)));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainDockWindow", nullptr, flags);

    renderHeader(stats, config);

    // Calculate heights
    float header_and_margin = 40.0f;
    float config_panel_h = 165.0f;
    float center_h = static_cast<float>(display_h) - config_panel_h - header_and_margin;
    if (center_h < 200.0f) center_h = 200.0f;

    float center_w = static_cast<float>(display_w) - 20.0f;
    float half_w = center_w * 0.5f;

    // Top Split (Left: Stopwatch, Right: Video)
    ImGui::BeginChild("CenterSplitLeft", ImVec2(half_w, center_h), false);
    renderStopwatchPanel(stopwatch);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CenterSplitRight", ImVec2(half_w, center_h), false);
    renderVideoPanel(stopwatch);
    ImGui::EndChild();

    // Bottom Panel
    renderConfigPanel(config, stats, engine_changed, stream_start_requested, stream_stop_requested, vsync_enabled);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Capture complete side-by-side canvas on snapshot request
    if (snapshot_requested_) {
        snapshot_requested_ = false;
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w > 0 && fb_h > 0) {
            std::vector<uint8_t> pixels(fb_w * fb_h * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            // Flip vertically because OpenGL coordinates start at bottom-left
            std::vector<uint8_t> flipped(fb_w * fb_h * 4);
            for (int y = 0; y < fb_h; ++y) {
                std::memcpy(
                    flipped.data() + y * fb_w * 4,
                    pixels.data() + (fb_h - 1 - y) * fb_w * 4,
                    fb_w * 4
                );
            }

            VideoFrame snap_frame;
            snap_frame.width = fb_w;
            snap_frame.height = fb_h;
            snap_frame.data = std::move(flipped);
            last_saved_path_ = ImageDumper::saveFramePng(snap_frame);
        }
    }
}

} // namespace g2g
