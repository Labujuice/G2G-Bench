#pragma once

#include <string>
#include <vector>

namespace g2g {

struct V4L2Profile {
    std::string format; // e.g. "MJPG", "YUYV"
    int width = 0;
    int height = 0;
    int fps = 0;
    std::string label;  // e.g. "MJPG  1280x720 @ 30 fps"
};

struct V4L2DeviceInfo {
    std::string path;        // e.g. "/dev/video0"
    std::string card_name;   // e.g. "HD User Facing"
    std::vector<V4L2Profile> profiles; // Valid combinations only
};

class V4L2Enumerator {
public:
    // Enumerate all available /dev/video* devices and their valid profile combinations
    static std::vector<V4L2DeviceInfo> enumerateDevices();

    // Query specific device details
    static bool queryDevice(const std::string& dev_path, V4L2DeviceInfo& info);
};

} // namespace g2g
