#pragma once

#include <Eigen/Dense>
#include "core/entities/MathTypes.h"
#include "core/entities/KalmanState.h"

namespace macroman {

/**
 * @brief Stateless Kalman filter for target prediction
 *
 * This implementation is stateless to support SoA architectures.
 * It takes a reference to a KalmanState POD which stores the
 * state vector and covariance matrix.
 */
class KalmanPredictor {
public:
    KalmanPredictor(float processNoise = 10.0f, float measurementNoise = 0.01f);

    /**
     * @brief Update state with a new observation
     * @param observation Measured position [x, y]
     * @param dt Time since last update
     * @param state State POD to update (stored in TargetDatabase)
     */
    void update(Vec2 observation, float dt, KalmanState& state) const noexcept;

    /**
     * @brief Advance state without a new observation (coasting)
     * @param dt Time to advance
     * @param state State POD to update
     */
    void predictState(float dt, KalmanState& state) const noexcept;

    /**
     * @brief Predict position ahead in time without modifying state
     * @param dt Time to predict ahead
     * @param state Current state POD
     */
    [[nodiscard]] Vec2 predict(float dt, const KalmanState& state) const noexcept;

private:
    float q_val_; // Process noise scaling
    float r_val_; // Measurement noise scaling

    // Internal Eigen mapping helpers
    static void toEigen(const KalmanState& state, Eigen::Vector4f& x, Eigen::Matrix4f& P) noexcept;
    static void fromEigen(const Eigen::Vector4f& x, const Eigen::Matrix4f& P, KalmanState& state) noexcept;
};

} // namespace macroman
