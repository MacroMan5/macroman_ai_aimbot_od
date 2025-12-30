#include "KalmanPredictor.h"
#include <algorithm>

namespace macroman {

KalmanPredictor::KalmanPredictor() {
    initKalman();
}

void KalmanPredictor::initKalman() {
    // State: [x, y, vx, vy]
    // Measurement: [x, y]
    kf_.init(4, 2, 0);

    // Transition matrix
    kf_.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);

    // Measurement matrix
    kf_.measurementMatrix = (cv::Mat_<float>(2, 4) <<
        1, 0, 0, 0,
        0, 1, 0, 0);

    // Process noise
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(processNoise_));

    // Measurement noise
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(measurementNoise_));

    // Initial covariance
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));
}

void KalmanPredictor::update(const cv::Point2f& position,
                             std::chrono::steady_clock::time_point timestamp) {
    // Add to history
    history_.push_back({position, timestamp});
    if (history_.size() > MAX_HISTORY) {
        history_.pop_front();
    }

    if (!initialized_) {
        // Initialize state with first measurement
        kf_.statePost.at<float>(0) = position.x;
        kf_.statePost.at<float>(1) = position.y;
        kf_.statePost.at<float>(2) = 0;
        kf_.statePost.at<float>(3) = 0;
        initialized_ = true;
        updateCount_ = 1;
        return;
    }

    // Predict then correct
    kf_.predict();

    cv::Mat measurement = (cv::Mat_<float>(2, 1) << position.x, position.y);
    kf_.correct(measurement);

    updateCount_++;
}

PredictedPosition KalmanPredictor::predict(std::chrono::milliseconds ahead) const {
    PredictedPosition result;
    result.timeAhead = ahead;

    if (!isStable()) {
        if (!history_.empty()) {
            result.position = history_.back().position;
        }
        result.confidence = 0.0f;
        return result;
    }

    // Get current state
    float x = kf_.statePost.at<float>(0);
    float y = kf_.statePost.at<float>(1);
    float vx = kf_.statePost.at<float>(2);
    float vy = kf_.statePost.at<float>(3);

    // Predict future position
    float dt = ahead.count() / 1000.0f;  // Convert to seconds
    result.position.x = x + vx * dt;
    result.position.y = y + vy * dt;

    // Confidence based on update count and prediction distance
    result.confidence = std::min(1.0f, updateCount_ / 10.0f) *
                       std::max(0.0f, 1.0f - dt);

    return result;
}

void KalmanPredictor::reset() {
    initialized_ = false;
    updateCount_ = 0;
    history_.clear();
    initKalman();
}

bool KalmanPredictor::isStable() const {
    return updateCount_ >= 3;
}

cv::Point2f KalmanPredictor::getVelocity() const {
    if (!initialized_) {
        return cv::Point2f(0, 0);
    }

    return cv::Point2f(
        kf_.statePost.at<float>(2),
        kf_.statePost.at<float>(3)
    );
}

void KalmanPredictor::setProcessNoise(float noise) {
    processNoise_ = noise;
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(noise));
}

void KalmanPredictor::setMeasurementNoise(float noise) {
    measurementNoise_ = noise;
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(noise));
}

} // namespace macroman
