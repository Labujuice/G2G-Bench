#include "Stopwatch.hpp"
#include <ctime>
#include <cstdio>
#include <chrono>

namespace g2g {

Stopwatch::Stopwatch() {
    reset();
}

uint64_t Stopwatch::getMonotonicRawUs() {
#if defined(CLOCK_MONOTONIC_RAW)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
        return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
    }
#endif
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

void Stopwatch::start() {
    if (!is_running_.load(std::memory_order_relaxed)) {
        start_time_us_ = getMonotonicRawUs();
        is_running_.store(true, std::memory_order_release);
    }
}

void Stopwatch::pause() {
    if (is_running_.load(std::memory_order_relaxed)) {
        uint64_t now = getMonotonicRawUs();
        accumulated_us_ += (now - start_time_us_);
        is_running_.store(false, std::memory_order_release);
    }
}

void Stopwatch::toggle() {
    if (isRunning()) {
        pause();
    } else {
        start();
    }
}

void Stopwatch::reset() {
    is_running_.store(false, std::memory_order_release);
    start_time_us_ = 0;
    accumulated_us_ = 0;
}

bool Stopwatch::isRunning() const {
    return is_running_.load(std::memory_order_acquire);
}

uint64_t Stopwatch::getElapsedMicroseconds() const {
    if (isRunning()) {
        uint64_t now = getMonotonicRawUs();
        return accumulated_us_ + (now - start_time_us_);
    }
    return accumulated_us_;
}

std::string Stopwatch::formatTimeMs(uint64_t elapsed_us) {
    uint64_t total_ms = elapsed_us / 1000ULL;
    uint32_t ms = static_cast<uint32_t>(total_ms % 1000ULL);
    uint32_t total_sec = static_cast<uint32_t>(total_ms / 1000ULL);
    uint32_t sec = total_sec % 60;
    uint32_t min = total_sec / 60;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02u:%02u.%03u", min, sec, ms);
    return std::string(buf);
}

std::string Stopwatch::formatTimeUs(uint64_t elapsed_us) {
    uint64_t total_us = elapsed_us;
    uint32_t us = static_cast<uint32_t>(total_us % 1000000ULL);
    uint32_t total_sec = static_cast<uint32_t>(total_us / 1000000ULL);
    uint32_t sec = total_sec % 60;
    uint32_t min = total_sec / 60;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02u:%02u.%06u", min, sec, us);
    return std::string(buf);
}

} // namespace g2g
