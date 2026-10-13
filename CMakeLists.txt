cmake_minimum_required(VERSION 3.22)
project(g2g_bench VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O3 -pthread)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find PkgConfig
find_package(PkgConfig REQUIRED)

# Find GStreamer
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

# Find FFmpeg
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
)

# Fetch or find GLFW
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
)
FetchContent_MakeAvailable(imgui)

# Create Dear ImGui library target
set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIR}
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Main Executable
add_executable(g2g_bench
    src/main.cpp
    src/core/Stopwatch.cpp
    src/utils/ImageDumper.cpp
    src/utils/V4L2Enumerator.cpp
    src/engine/GstVideoEngine.cpp
    src/engine/NativeVideoEngine.cpp
    src/render/GLTexture.cpp
    src/render/UIRenderer.cpp
)

target_include_directories(g2g_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GST_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
)

target_link_libraries(g2g_bench PRIVATE
    imgui
    glfw
    OpenGL::GL
    ${GST_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    pthread
    dl
)

target_compile_options(g2g_bench PRIVATE
    ${GST_CFLAGS_OTHER}
    ${FFMPEG_CFLAGS_OTHER}
)
