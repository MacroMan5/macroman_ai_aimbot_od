#pragma once

#include "Detection.h"
#include <opencv2/core/types.hpp>
#include <chrono>

namespace sunone {

struct Target {
    Detection detection;
    cv::Point2f position;      // Current center
    cv::Point2f velocity;      // Estimated velocity
    cv::Point2f pivotPoint;    // Aim point (head, center, etc.)

    // Tracking
    int trackingId;
    int framesTracked;
    float trackingConfidence;

    Target() : trackingId(-1), framesTracked(0), trackingConfidence(0.0f) {}

    explicit Target(const Detection& det)
        : detection(det)
        , position(static_cast<float>(det.center().x), static_cast<float>(det.center().y))
        , velocity(0, 0)
        , pivotPoint(static_cast<float>(det.center().x), static_cast<float>(det.center().y))
        , trackingId(-1)
        , framesTracked(1)
        , trackingConfidence(det.confidence) {}
};

struct PredictedPosition {
    cv::Point2f position;
    float confidence;
    std::chrono::milliseconds timeAhead;
};

} // namespace sunone
