#include "V4L2Enumerator.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <algorithm>
#include <set>
#include <iostream>

namespace g2g {

static std::string fourccToString(uint32_t fourcc) {
    if (fourcc == V4L2_PIX_FMT_YUYV) return "YUYV";
    if (fourcc == V4L2_PIX_FMT_MJPEG) return "MJPEG";
    if (fourcc == V4L2_PIX_FMT_NV12) return "NV12";
    if (fourcc == V4L2_PIX_FMT_H264) return "H264";

    char buf[5] = {0};
    buf[0] = static_cast<char>(fourcc & 0xFF);
    buf[1] = static_cast<char>((fourcc >> 8) & 0xFF);
    buf[2] = static_cast<char>((fourcc >> 16) & 0xFF);
    buf[3] = static_cast<char>((fourcc >> 24) & 0xFF);
    return std::string(buf);
}

bool V4L2Enumerator::queryDevice(const std::string& dev_path, V4L2DeviceInfo& info) {
    int fd = open(dev_path.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        return false;
    }

    struct v4l2_capability cap;
    std::memset(&cap, 0, sizeof(cap));
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        return false;
    }

    uint32_t caps = cap.capabilities;
    if (caps & V4L2_CAP_DEVICE_CAPS) {
        caps = cap.device_caps;
    }

    if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
        close(fd);
        return false;
    }

    info.path = dev_path;
    info.card_name = reinterpret_cast<const char*>(cap.card);
    info.supported_formats.clear();
    info.supported_resolutions.clear();
    info.supported_framerates.clear();

    std::set<std::string> format_set;
    std::set<std::pair<int, int>> resolution_set;
    std::set<int> fps_set;

    struct v4l2_fmtdesc fmtdesc;
    std::memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        std::string fmt_name = fourccToString(fmtdesc.pixelformat);
        format_set.insert(fmt_name);

        struct v4l2_frmsizeenum frmsize;
        std::memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmtdesc.pixelformat;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                int w = static_cast<int>(frmsize.discrete.width);
                int h = static_cast<int>(frmsize.discrete.height);
                resolution_set.insert({w, h});

                struct v4l2_frmivalenum frmival;
                std::memset(&frmival, 0, sizeof(frmival));
                frmival.pixel_format = fmtdesc.pixelformat;
                frmival.width = frmsize.discrete.width;
                frmival.height = frmsize.discrete.height;

                while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                    if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if (frmival.discrete.numerator > 0) {
                            int fps = static_cast<int>(frmival.discrete.denominator / frmival.discrete.numerator);
                            if (fps > 0) {
                                fps_set.insert(fps);
                            }
                        }
                    }
                    frmival.index++;
                }
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    close(fd);

    for (const auto& fmt : format_set) {
        info.supported_formats.push_back(fmt);
    }

    // Sort resolutions descending (e.g. 1920x1080 first)
    info.supported_resolutions.assign(resolution_set.begin(), resolution_set.end());
    std::sort(info.supported_resolutions.begin(), info.supported_resolutions.end(), [](const auto& a, const auto& b) {
        return (a.first * a.second) > (b.first * b.second);
    });

    // Sort framerates descending (e.g. 60, 30, ...)
    info.supported_framerates.assign(fps_set.begin(), fps_set.end());
    std::sort(info.supported_framerates.begin(), info.supported_framerates.end(), std::greater<int>());

    // Defaults if empty
    if (info.supported_formats.empty()) {
        info.supported_formats = {"YUYV", "MJPEG"};
    }
    if (info.supported_resolutions.empty()) {
        info.supported_resolutions = {{1920, 1080}, {1280, 720}, {640, 480}};
    }
    if (info.supported_framerates.empty()) {
        info.supported_framerates = {60, 30, 15};
    }

    return true;
}

std::vector<V4L2DeviceInfo> V4L2Enumerator::enumerateDevices() {
    std::vector<V4L2DeviceInfo> devices;

    for (int i = 0; i < 32; ++i) {
        std::string dev_path = "/dev/video" + std::to_string(i);
        V4L2DeviceInfo info;
        if (queryDevice(dev_path, info)) {
            devices.push_back(info);
        }
    }

    if (devices.empty()) {
        V4L2DeviceInfo fallback;
        fallback.path = "/dev/video0";
        fallback.card_name = "Default Camera (/dev/video0)";
        fallback.supported_formats = {"YUYV", "MJPEG"};
        fallback.supported_resolutions = {{1920, 1080}, {1280, 720}, {640, 480}};
        fallback.supported_framerates = {60, 30, 15};
        devices.push_back(fallback);
    }

    return devices;
}

} // namespace g2g
