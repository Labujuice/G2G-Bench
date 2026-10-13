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
    if (fourcc == V4L2_PIX_FMT_MJPEG) return "MJPG";
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
    info.profiles.clear();

    struct v4l2_fmtdesc fmtdesc;
    std::memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    std::set<std::string> seen_profiles;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        std::string fmt_name = fourccToString(fmtdesc.pixelformat);

        struct v4l2_frmsizeenum frmsize;
        std::memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmtdesc.pixelformat;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                int w = static_cast<int>(frmsize.discrete.width);
                int h = static_cast<int>(frmsize.discrete.height);

                struct v4l2_frmivalenum frmival;
                std::memset(&frmival, 0, sizeof(frmival));
                frmival.pixel_format = fmtdesc.pixelformat;
                frmival.width = frmsize.discrete.width;
                frmival.height = frmsize.discrete.height;

                bool had_fps = false;
                while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                    if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if (frmival.discrete.numerator > 0) {
                            int fps = static_cast<int>(frmival.discrete.denominator / frmival.discrete.numerator);
                            if (fps > 0) {
                                std::string key = fmt_name + "_" + std::to_string(w) + "_" + std::to_string(h) + "_" + std::to_string(fps);
                                if (seen_profiles.find(key) == seen_profiles.end()) {
                                    seen_profiles.insert(key);

                                    V4L2Profile prof;
                                    prof.format = fmt_name;
                                    prof.width = w;
                                    prof.height = h;
                                    prof.fps = fps;
                                    prof.label = fmt_name + " - " + std::to_string(w) + "x" + std::to_string(h) + " @ " + std::to_string(fps) + " fps";
                                    info.profiles.push_back(prof);
                                    had_fps = true;
                                }
                            }
                        }
                    }
                    frmival.index++;
                }

                if (!had_fps) {
                    std::string key = fmt_name + "_" + std::to_string(w) + "_" + std::to_string(h) + "_30";
                    if (seen_profiles.find(key) == seen_profiles.end()) {
                        seen_profiles.insert(key);
                        V4L2Profile prof;
                        prof.format = fmt_name;
                        prof.width = w;
                        prof.height = h;
                        prof.fps = 30;
                        prof.label = fmt_name + " - " + std::to_string(w) + "x" + std::to_string(h) + " @ 30 fps";
                        info.profiles.push_back(prof);
                    }
                }
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    close(fd);

    // Sort profiles: highest pixel throughput first (Resolution * FPS), then by format
    std::sort(info.profiles.begin(), info.profiles.end(), [](const V4L2Profile& a, const V4L2Profile& b) {
        uint64_t rate_a = static_cast<uint64_t>(a.width) * a.height * a.fps;
        uint64_t rate_b = static_cast<uint64_t>(b.width) * b.height * b.fps;
        if (rate_a != rate_b) return rate_a > rate_b;
        if ((a.width * a.height) != (b.width * b.height)) return (a.width * a.height) > (b.width * b.height);
        return a.format < b.format;
    });

    // Provide default fallback profile if none found
    if (info.profiles.empty()) {
        info.profiles.push_back({"MJPG", 1280, 720, 30, "MJPG - 1280x720 @ 30 fps"});
        info.profiles.push_back({"YUYV", 1280, 720, 10, "YUYV - 1280x720 @ 10 fps"});
        info.profiles.push_back({"YUYV", 640, 480, 30, "YUYV - 640x480 @ 30 fps"});
        info.profiles.push_back({"MJPG", 640, 480, 30, "MJPG - 640x480 @ 30 fps"});
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
        fallback.profiles.push_back({"MJPG", 1280, 720, 30, "MJPG - 1280x720 @ 30 fps"});
        fallback.profiles.push_back({"YUYV", 1280, 720, 10, "YUYV - 1280x720 @ 10 fps"});
        fallback.profiles.push_back({"YUYV", 640, 480, 30, "YUYV - 640x480 @ 30 fps"});
        devices.push_back(fallback);
    }

    return devices;
}

} // namespace g2g
