#include "KalmanPredictor.h"

namespace macroman {

KalmanPredictor::KalmanPredictor(float processNoise, float measurementNoise)
    : q_val_(processNoise), r_val_(measurementNoise) {}

void KalmanPredictor::toEigen(const KalmanState& state, Eigen::Vector4f& x, Eigen::Matrix4f& P) noexcept {
    x << state.x, state.y, state.vx, state.vy;
    for (int i = 0; i < 16; ++i) P(i) = state.covariance[i];
}

void KalmanPredictor::fromEigen(const Eigen::Vector4f& x, const Eigen::Matrix4f& P, KalmanState& state) noexcept {
    state.x = x(0); state.y = x(1); state.vx = x(2); state.vy = x(3);
    for (int i = 0; i < 16; ++i) state.covariance[i] = P(i);
}

void KalmanPredictor::update(Vec2 observation, float dt, KalmanState& state) const noexcept {
    Eigen::Vector4f x; Eigen::Matrix4f P;
    toEigen(state, x, P);

    // 1. Prediction step
    Eigen::Matrix4f F;
    F << 1, 0, dt, 0,
         0, 1, 0,  dt,
         0, 0, 1,  0,
         0, 0, 0,  1;

    Eigen::Matrix4f Q = Eigen::Matrix4f::Identity() * q_val_;
    x = F * x;
    P = F * P * F.transpose() + Q;

    // 2. Correction step
    Eigen::Matrix<float, 2, 4> H;
    H << 1, 0, 0, 0,
         0, 1, 0, 0;

    Eigen::Matrix2f R = Eigen::Matrix2f::Identity() * r_val_;
    Eigen::Vector2f z(observation.x, observation.y);
    Eigen::Vector2f y = z - H * x;
    Eigen::Matrix2f S = H * P * H.transpose() + R;
    Eigen::Matrix<float, 4, 2> K = P * H.transpose() * S.inverse();

    x = x + K * y;
    P = (Eigen::Matrix4f::Identity() - K * H) * P;

    fromEigen(x, P, state);
}

void KalmanPredictor::predictState(float dt, KalmanState& state) const noexcept {
    Eigen::Vector4f x; Eigen::Matrix4f P;
    toEigen(state, x, P);

    // Prediction step only
    Eigen::Matrix4f F;
    F << 1, 0, dt, 0,
         0, 1, 0,  dt,
         0, 0, 1,  0,
         0, 0, 0,  1;

    Eigen::Matrix4f Q = Eigen::Matrix4f::Identity() * q_val_;
    x = F * x;
    P = F * P * F.transpose() + Q;

    fromEigen(x, P, state);
}

Vec2 KalmanPredictor::predict(float dt, const KalmanState& state) const noexcept {
    return {
        state.x + state.vx * dt,
        state.y + state.vy * dt
    };
}

} // namespace macroman
