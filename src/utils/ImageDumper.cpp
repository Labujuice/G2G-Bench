#include "ImageDumper.hpp"
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb_image_write.h"

namespace g2g {

static void ensureDirectoryExists(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

static std::string generateTimestampFilename(const std::string& prefix) {
    ensureDirectoryExists("captures");
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_buf);
    return "captures/" + prefix + "_" + std::string(buf) + ".png";
}

std::string ImageDumper::saveFramePng(const VideoFrame& frame, const std::string& output_filepath) {
    if (frame.data.empty() || frame.width <= 0 || frame.height <= 0) {
        std::cerr << "[ImageDumper] Frame is empty, cannot save PNG." << std::endl;
        return "";
    }

    std::string path = output_filepath;
    if (path.empty()) {
        path = generateTimestampFilename("frame");
    }

    int stride = frame.width * 4;
    int success = stbi_write_png(path.c_str(), frame.width, frame.height, 4, frame.data.data(), stride);
    if (!success) {
        std::cerr << "[ImageDumper] Failed to write PNG to: " << path << std::endl;
        return "";
    }

    std::cout << "[ImageDumper] Successfully saved frame to: " << path << std::endl;
    return path;
}

std::string ImageDumper::saveComparisonPng(const std::string& ref_time_str, const VideoFrame& frame, const std::string& output_filepath) {
    if (frame.data.empty() || frame.width <= 0 || frame.height <= 0) {
        return saveFramePng(frame, output_filepath);
    }

    std::string path = output_filepath;
    if (path.empty()) {
        path = generateTimestampFilename("comparison");
    }

    // Create a canvas with a header or side banner containing ref_time_str
    int header_height = 80;
    int canvas_width = frame.width;
    int canvas_height = frame.height + header_height;
    std::vector<uint8_t> canvas(canvas_width * canvas_height * 4, 0);

    // Fill header background (Dark grey)
    for (int y = 0; y < header_height; ++y) {
        for (int x = 0; x < canvas_width; ++x) {
            int idx = (y * canvas_width + x) * 4;
            canvas[idx + 0] = 30;  // R
            canvas[idx + 1] = 30;  // G
            canvas[idx + 2] = 35;  // B
            canvas[idx + 3] = 255; // A
        }
    }

    // Copy frame pixels below header
    const uint8_t* src = frame.data.data();
    uint8_t* dst = canvas.data() + (header_height * canvas_width * 4);
    std::memcpy(dst, src, frame.width * frame.height * 4);

    int stride = canvas_width * 4;
    int success = stbi_write_png(path.c_str(), canvas_width, canvas_height, 4, canvas.data(), stride);
    if (!success) {
        std::cerr << "[ImageDumper] Failed to write comparison PNG: " << path << std::endl;
        return "";
    }

    std::cout << "[ImageDumper] Successfully saved comparison snapshot (" << ref_time_str << ") to: " << path << std::endl;
    return path;
}

} // namespace g2g
