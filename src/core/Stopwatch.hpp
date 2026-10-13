#pragma once

#include <cstdint>
#include <string>
#include <atomic>

namespace g2g {

class Stopwatch {
public:
    Stopwatch();
    ~Stopwatch() = default;

    void start();
    void pause();
    void toggle();
    void reset();

    bool isRunning() const;

    // Get current elapsed time in microseconds
    uint64_t getElapsedMicroseconds() const;

    // Format microseconds into "MM:SS.mmm"
    static std::string formatTimeMs(uint64_t elapsed_us);

    // Format microseconds into "MM:SS.ffffff"
    static std::string formatTimeUs(uint64_t elapsed_us);

    // Get current timestamp from CLOCK_MONOTONIC_RAW in microseconds
    static uint64_t getMonotonicRawUs();

private:
    std::atomic<bool> is_running_{false};
    uint64_t start_time_us_{0};
    uint64_t accumulated_us_{0};
};

} // namespace g2g
