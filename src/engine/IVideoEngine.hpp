#pragma once

#include "core/Types.hpp"
#include <memory>

namespace g2g {

class IVideoEngine {
public:
    virtual ~IVideoEngine() = default;

    /**
     * @brief Start video ingestion pipeline
     * @param config Pipeline configuration
     * @return true if successfully initiated
     */
    virtual bool start(const EngineConfig& config) = 0;

    /**
     * @brief Stop video ingestion pipeline and release resources
     */
    virtual void stop() = 0;

    /**
     * @brief Check if the engine is running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Retrieve the latest frame received (drops older unread frames)
     * @param out_frame Output frame buffer
     * @return true if a fresh frame was fetched, false if no new frame
     */
    virtual bool getLatestFrame(VideoFrame& out_frame) = 0;

    /**
     * @brief Get runtime statistics (FPS, drops, status)
     */
    virtual EngineStats getStats() const = 0;
};

} // namespace g2g
