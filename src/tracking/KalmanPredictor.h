#pragma once

#include "core/interfaces/ITargetPredictor.h"
#include <opencv2/video/tracking.hpp>
#include <chrono>
#include <deque>

namespace macroman {

/**
 * @class KalmanPredictor
 * @brief Uses a linear Kalman Filter to predict target movement.
 *
 * Implements a Constant Velocity (CV) model to smooth tracking data and predict
 * future positions. This is effective for targets moving in relatively straight lines
 * or smooth curves.
 *
 * State Vector (4 dimensions):
 * [0] x: Position X
 * [1] y: Position Y
 * [2] vx: Velocity X
 * [3] vy: Velocity Y
 *
 * Measurement Vector (2 dimensions):
 * [0] x: Measured Position X
 * [1] y: Measured Position Y
 */
class KalmanPredictor : public ITargetPredictor {
public:
    KalmanPredictor();
    ~KalmanPredictor() override = default;

    /**
     * @brief Updates the filter with a new observation.
     *
     * @param position The center point of the detected target (screen coordinates).
     * @param timestamp The time the frame was captured (currently unused for dt, assumes ~60fps).
     */
    void update(const cv::Point2f& position,
               std::chrono::steady_clock::time_point timestamp) override;

    /**
     * @brief Predicts the target's position at a future time.
     *
     * Uses the current state estimate and velocity to project the position forward.
     *
     * Formula:
     * x_future = x_current + (vx * dt)
     * y_future = y_current + (vy * dt)
     *
     * @param ahead The time duration to predict into the future.
     * @return PredictedPosition containing the (x,y) and a confidence score [0.0 - 1.0].
     *         Confidence is based on the number of stable updates and the prediction distance.
     */
    PredictedPosition predict(std::chrono::milliseconds ahead) const override;

    /**
     * @brief Resets the filter state.
     * Call this when a target is lost or a new target is selected to prevent
     * "ghost" momentum from the previous track.
     */
    void reset() override;

    std::string getName() const override { return "Kalman"; }
    
    /**
     * @brief Checks if the filter has converged enough to be reliable.
     * @return true if update count >= 3.
     */
    bool isStable() const override;

    /**
     * @brief Gets the current estimated velocity vector.
     * @return (vx, vy) in pixels per time step.
     */
    cv::Point2f getVelocity() const;

    /**
     * @brief Sets the process noise covariance (Q).
     * Higher values = filter trusts new measurements more (more jittery, faster response).
     * Lower values = filter trusts the model more (smoother, slower response).
     */
    void setProcessNoise(float noise);

    /**
     * @brief Sets the measurement noise covariance (R).
     * Represents the uncertainty in the detection bounding boxes.
     */
    void setMeasurementNoise(float noise);

private:
    void initKalman();

    cv::KalmanFilter kf_;
    bool initialized_ = false;
    int updateCount_ = 0;

    // History for velocity calculation
    struct Sample {
        cv::Point2f position;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::deque<Sample> history_;
    static constexpr size_t MAX_HISTORY = 10;

    float processNoise_ = 1e-4f;
    float measurementNoise_ = 1e-2f;
};

} // namespace macroman