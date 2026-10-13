#pragma once

#include <string>
#include <vector>
#include <utility>

namespace g2g {

struct V4L2DeviceInfo {
    std::string path;        // e.g. "/dev/video0"
    std::string card_name;   // e.g. "HD User Facing"
    std::vector<std::string> supported_formats; // e.g. ["YUYV", "MJPEG"]
    std::vector<std::pair<int, int>> supported_resolutions; // e.g. [(1280, 720), (640, 480)]
    std::vector<int> supported_framerates; // e.g. [60, 30, 10]
};

class V4L2Enumerator {
public:
    // Enumerate all available /dev/video* devices and their capabilities
    static std::vector<V4L2DeviceInfo> enumerateDevices();

    // Query specific device details (formats, resolutions, framerates)
    static bool queryDevice(const std::string& dev_path, V4L2DeviceInfo& info);
};

} // namespace g2g
