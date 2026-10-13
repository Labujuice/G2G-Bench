#pragma once

#include "core/Types.hpp"
#include <string>

namespace g2g {

class ImageDumper {
public:
    /**
     * @brief Save a single RGBA frame to PNG file
     * @param frame VideoFrame to save
     * @param output_filepath Path to save file. If empty, generates automatic filename.
     * @return Generated/used filepath on success, empty string on failure.
     */
    static std::string saveFramePng(const VideoFrame& frame, const std::string& output_filepath = "");

    /**
     * @brief Save combined side-by-side snapshot (Reference stopwatch text block + right frame)
     * @param ref_time_str Stopwatch formatted time string (e.g. "00:14.382")
     * @param frame Right video frame
     * @param output_filepath Optional path
     * @return Output filepath on success, empty on failure
     */
    static std::string saveComparisonPng(const std::string& ref_time_str, const VideoFrame& frame, const std::string& output_filepath = "");
};

} // namespace g2g
