#pragma once

#include <opencv2/core/types.hpp>
#include <vector>
#include <chrono>

namespace macroman {

struct Detection {
    cv::Rect box;
    float confidence;
    int classId;
    std::chrono::steady_clock::time_point captureTimestamp;

    // Helpers
    cv::Point center() const {
        return cv::Point(box.x + box.width / 2, box.y + box.height / 2);
    }

    float area() const {
        return static_cast<float>(box.width * box.height);
    }
};

using DetectionList = std::vector<Detection>;

} // namespace macroman
