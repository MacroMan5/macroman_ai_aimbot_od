#pragma once

#include <array>

namespace macroman {

/**
 * @brief Kalman filter state (constant velocity model)
 *
 * STATE VECTOR: [x, y, vx, vy]
 * - x, y: Position in pixels
 * - vx, vy: Velocity in pixels/second
 *
 * COVARIANCE MATRIX P (4x4):
 * - Represented as flat array for SoA storage
 */
struct KalmanState {
    float x{0.0f};   // Position X (pixels)
    float y{0.0f};   // Position Y (pixels)
    float vx{0.0f};  // Velocity X (pixels/second)
    float vy{0.0f};  // Velocity Y (pixels/second)

    // Flat covariance matrix (4x4 = 16 elements)
    // Initialized to high uncertainty
    std::array<float, 16> covariance{};

    KalmanState() {
        covariance.fill(0.0f);
        // Identity * 1000.0f
        covariance[0] = 1000.0f;
        covariance[5] = 1000.0f;
        covariance[10] = 1000.0f;
        covariance[15] = 1000.0f;
    }
};

} // namespace macroman
