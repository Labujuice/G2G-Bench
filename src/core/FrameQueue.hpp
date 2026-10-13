#pragma once

#include "Types.hpp"
#include <array>
#include <atomic>
#include <cstdint>

namespace g2g {

/**
 * @brief Lock-free triple-buffer exchanger designed for zero-latency frame passing.
 * The producer writes to write_idx, publishes it to ready_idx.
 * The consumer retrieves ready_idx into read_idx.
 * If producer writes faster than consumer reads, older frames are automatically dropped without queuing.
 */
class FrameQueue {
public:
    FrameQueue() {
        write_idx_ = 0;
        ready_idx_.store(1, std::memory_order_relaxed);
        read_idx_ = 2;
        has_new_frame_.store(false, std::memory_order_relaxed);
        dropped_count_.store(0, std::memory_order_relaxed);
        total_pushed_.store(0, std::memory_order_relaxed);
    }

    // Producer obtains a buffer to write
    VideoFrame& getWriteBuffer() {
        return buffers_[write_idx_];
    }

    // Producer publishes the written frame
    void publishFrame() {
        total_pushed_.fetch_add(1, std::memory_order_relaxed);
        if (has_new_frame_.load(std::memory_order_relaxed)) {
            // A previous frame was not consumed yet -> count as dropped
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
        }
        
        // Atomically exchange write_idx_ with ready_idx_
        int old_ready = ready_idx_.exchange(write_idx_, std::memory_order_acq_rel);
        write_idx_ = old_ready;
        has_new_frame_.store(true, std::memory_order_release);
    }

    // Consumer checks and retrieves the latest frame if available
    bool acquireLatestFrame(VideoFrame& out_frame) {
        if (!has_new_frame_.exchange(false, std::memory_order_acq_rel)) {
            return false;
        }

        // Atomically exchange read_idx_ with ready_idx_
        int latest = ready_idx_.exchange(read_idx_, std::memory_order_acq_rel);
        read_idx_ = latest;

        // Copy frame to output
        out_frame = buffers_[read_idx_];
        return true;
    }

    // Peek at current read buffer without waiting for new one
    const VideoFrame& getCurrentFrame() const {
        return buffers_[read_idx_];
    }

    uint64_t getDroppedCount() const {
        return dropped_count_.load(std::memory_order_relaxed);
    }

    uint64_t getTotalPushed() const {
        return total_pushed_.load(std::memory_order_relaxed);
    }

    void resetStats() {
        dropped_count_.store(0, std::memory_order_relaxed);
        total_pushed_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<VideoFrame, 3> buffers_;
    int write_idx_{0};
    std::atomic<int> ready_idx_{1};
    int read_idx_{2};
    std::atomic<bool> has_new_frame_{false};
    std::atomic<uint64_t> dropped_count_{0};
    std::atomic<uint64_t> total_pushed_{0};
};

} // namespace g2g
