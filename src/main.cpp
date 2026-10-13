#include "core/Stopwatch.hpp"
#include "core/Types.hpp"
#include "engine/GstVideoEngine.hpp"
#include "engine/NativeVideoEngine.hpp"
#include "render/UIRenderer.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>

static g2g::UIRenderer* g_ui_renderer = nullptr;
static g2g::Stopwatch* g_stopwatch = nullptr;

static void keyCallback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/) {
    if (g_ui_renderer && g_stopwatch) {
        g_ui_renderer->handleKeyPress(key, action, *g_stopwatch);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1440, 880, "G2G-Bench - Low Latency Benchmark Tool", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Default VSync on (1) to prevent tearing, user can toggle to 0 for unlimited fps
    bool vsync_enabled = true;
    glfwSwapInterval(1);

    g2g::Stopwatch stopwatch;
    g2g::UIRenderer ui_renderer;
    if (!ui_renderer.init(window)) {
        std::cerr << "Failed to initialize UI Renderer" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    g_ui_renderer = &ui_renderer;
    g_stopwatch = &stopwatch;
    glfwSetKeyCallback(window, keyCallback);

    // Video Engines
    auto gst_engine = std::make_unique<g2g::GstVideoEngine>();
    auto native_engine = std::make_unique<g2g::NativeVideoEngine>();

    g2g::EngineConfig config;
    config.engine_type = g2g::EngineType::GSTREAMER;
    config.source_type = g2g::SourceType::UDP_RTP;
    config.udp_port = 5004;

    g2g::IVideoEngine* current_engine = gst_engine.get();

    // Auto-start stream on default config
    current_engine->start(config);

    g2g::VideoFrame frame;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        bool engine_changed = false;
        bool stream_start_requested = false;
        bool stream_stop_requested = false;

        // Fetch latest frame without blocking
        if (current_engine && current_engine->isRunning()) {
            if (current_engine->getLatestFrame(frame)) {
                ui_renderer.updateVideoFrame(frame);
            }
        }

        // Render OpenGL & ImGui
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ui_renderer.render(
            window,
            stopwatch,
            current_engine,
            config,
            engine_changed,
            stream_start_requested,
            stream_stop_requested,
            vsync_enabled
        );

        glfwSwapBuffers(window);

        // Handle dynamic engine / source switching
        if (engine_changed || stream_start_requested) {
            if (current_engine) {
                current_engine->stop();
            }

            if (config.engine_type == g2g::EngineType::GSTREAMER) {
                current_engine = gst_engine.get();
            } else {
                current_engine = native_engine.get();
            }

            current_engine->start(config);
        } else if (stream_stop_requested) {
            if (current_engine) {
                current_engine->stop();
            }
        }
    }

    if (current_engine) {
        current_engine->stop();
    }

    ui_renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
