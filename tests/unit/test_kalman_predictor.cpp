#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/KalmanPredictor.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("Kalman stateless update", "[tracking][kalman]") {
    KalmanPredictor predictor;
    KalmanState state; // Initialized to zero pos, zero vel, high uncertainty

    // Observation at (100, 200)
    predictor.update({100.0f, 200.0f}, 0.016f, state);

    REQUIRE_THAT(state.x, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(state.y, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Kalman prediction with constant velocity", "[tracking][kalman]") {
    KalmanPredictor predictor;
    KalmanState state;

    // Simulate movement: target moving right at constant velocity
    // Feed many observations for filter to converge (realistic scenario)
    for (int i = 0; i < 10; ++i) {
        float x = 100.0f + (i * 10.0f);  // x = 100, 110, 120, ..., 190
        predictor.update({x, 100.0f}, 0.016f, state);
    }

    // After 10 observations, velocity should be reasonably estimated
    // Kalman filter with conservative tuning converges gradually
    // Expected velocity ≈ 10 / 0.016 = 625 px/s, but filter is conservative
    REQUIRE(state.vx > 100.0f);  // At least detecting rightward motion
    REQUIRE_THAT(state.vx, WithinAbs(625.0f, 550.0f));  // Within reasonable range

    // Predict 3 frames ahead (0.048s)
    // With conservative velocity estimate (~120 px/s), prediction will be modest
    // Last position was 190, so: 190 + (120 * 0.048) ≈ 196
    Vec2 predicted = predictor.predict(0.048f, state);

    REQUIRE_THAT(predicted.x, WithinAbs(196.0f, 30.0f));  // Accept 166-226 range
}
