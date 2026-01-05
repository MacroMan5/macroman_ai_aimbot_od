#pragma once

#include "../entities/Target.h"
#include <chrono>
#include <string>

namespace macroman {

class ITargetPredictor {
public:
    virtual ~ITargetPredictor() = default;

    // Update with new observed position
    virtual void update(const cv::Point2f& position,
                       std::chrono::steady_clock::time_point timestamp) = 0;

    // Predict future position
    virtual PredictedPosition predict(std::chrono::milliseconds ahead) const = 0;

    // Reset state
    virtual void reset() = 0;

    // Info
    virtual std::string getName() const = 0;
    virtual bool isStable() const = 0;  // Enough data to predict?
};

} // namespace macroman
