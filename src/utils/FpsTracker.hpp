#pragma once

#include <cstdint>
#include <chrono>

namespace g2g {

class FpsTracker {
public:
    explicit FpsTracker(double window_sec = 0.5) 
        : window_duration_us_(static_cast<uint64_t>(window_sec * 1000000.0)) {}

    void tick(uint64_t now_us) {
        if (last_tick_us_ == 0) {
            last_tick_us_ = now_us;
            window_start_us_ = now_us;
            return;
        }

        frames_in_window_++;
        uint64_t elapsed = now_us - window_start_us_;

        if (elapsed >= window_duration_us_) {
            current_fps_ = (static_cast<double>(frames_in_window_) * 1000000.0) / static_cast<double>(elapsed);
            frames_in_window_ = 0;
            window_start_us_ = now_us;
        }

        last_tick_us_ = now_us;
    }

    double getFps() const {
        return current_fps_;
    }

    void reset() {
        frames_in_window_ = 0;
        window_start_us_ = 0;
        last_tick_us_ = 0;
        current_fps_ = 0.0;
    }

private:
    uint64_t window_duration_us_;
    uint64_t frames_in_window_{0};
    uint64_t window_start_us_{0};
    uint64_t last_tick_us_{0};
    double current_fps_{0.0};
};

} // namespace g2g
