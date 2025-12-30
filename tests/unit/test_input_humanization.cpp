#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "input/movement/BezierCurve.h"
#include "input/humanization/Humanizer.h"
#include <cmath>

using namespace macroman;
using Catch::Matchers::WithinAbs;

// =============================================================================
// BezierCurve Overshoot Tests
// =============================================================================

TEST_CASE("BezierCurve normal phase (t in [0.0, 1.0])", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};      // Start
    curve.p1 = {10.0f, 0.0f};     // Control point 1
    curve.p2 = {20.0f, 0.0f};     // Control point 2
    curve.p3 = {30.0f, 0.0f};     // End (target)

    // At t=0.0, should be at start
    auto p0 = curve.at(0.0f);
    REQUIRE_THAT(p0.x, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(p0.y, WithinAbs(0.0f, 0.01f));

    // At t=0.5, should be midway
    auto p_mid = curve.at(0.5f);
    REQUIRE(p_mid.x > 0.0f);
    REQUIRE(p_mid.x < 30.0f);

    // At t=1.0, should be exactly at target
    auto p1 = curve.at(1.0f);
    REQUIRE_THAT(p1.x, WithinAbs(30.0f, 0.01f));
    REQUIRE_THAT(p1.y, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("BezierCurve overshoot phase (t in [1.0, 1.15])", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 0.0f};
    curve.p2 = {20.0f, 0.0f};
    curve.p3 = {30.0f, 0.0f};
    curve.overshootFactor = 0.15f;  // 15% overshoot

    // At t=1.0, should be at target (start of overshoot phase)
    auto p_start = curve.at(1.0f);
    REQUIRE_THAT(p_start.x, WithinAbs(30.0f, 0.01f));

    // At t=1.075 (peak overshoot), should be beyond target
    auto p_peak = curve.at(1.075f);
    REQUIRE(p_peak.x > 30.0f);  // Overshoot past target

    // Maximum overshoot should be approximately 15% of distance from p2 to p3
    float expectedMaxOvershoot = 30.0f + (30.0f - 20.0f) * 0.15f;  // 30 + 1.5 = 31.5
    REQUIRE_THAT(p_peak.x, WithinAbs(expectedMaxOvershoot, 2.0f));  // Allow 2px tolerance

    // At t=1.15, should be back at target (correction complete)
    auto p_end = curve.at(1.15f);
    REQUIRE_THAT(p_end.x, WithinAbs(30.0f, 0.1f));
    REQUIRE_THAT(p_end.y, WithinAbs(0.0f, 0.1f));
}

TEST_CASE("BezierCurve clamping behavior", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 0.0f};
    curve.p2 = {20.0f, 0.0f};
    curve.p3 = {30.0f, 0.0f};

    // Below lower bound (t < 0.0) should clamp to start
    auto p_neg = curve.at(-0.5f);
    REQUIRE_THAT(p_neg.x, WithinAbs(0.0f, 0.01f));

    // Above upper bound (t > 1.15) should clamp to target
    auto p_high = curve.at(2.0f);
    REQUIRE_THAT(p_high.x, WithinAbs(30.0f, 0.01f));
}

TEST_CASE("BezierCurve overshoot with zero movement", "[input][bezier]") {
    BezierCurve curve;
    // All points at same location (no movement)
    curve.p0 = {100.0f, 100.0f};
    curve.p1 = {100.0f, 100.0f};
    curve.p2 = {100.0f, 100.0f};
    curve.p3 = {100.0f, 100.0f};

    // Should handle zero movement gracefully (no overshoot)
    auto p = curve.at(1.075f);
    REQUIRE_THAT(p.x, WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(p.y, WithinAbs(100.0f, 0.01f));
}

TEST_CASE("BezierCurve length estimation", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 0.0f};
    curve.p2 = {20.0f, 0.0f};
    curve.p3 = {30.0f, 0.0f};

    // Chord length approximation: |p1-p0| + |p2-p1| + |p3-p2|
    float expectedLength = 10.0f + 10.0f + 10.0f;  // 30.0
    REQUIRE_THAT(curve.length(), WithinAbs(expectedLength, 0.01f));
}

// =============================================================================
// Humanizer Reaction Delay Tests
// =============================================================================

TEST_CASE("Humanizer reaction delay within bounds", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableReactionDelay = true;
    config.reactionDelayMean = 160.0f;
    config.reactionDelayStdDev = 25.0f;
    config.reactionDelayMin = 100.0f;
    config.reactionDelayMax = 300.0f;

    Humanizer humanizer(config);

    // Sample 100 delays, all should be within [100ms, 300ms]
    for (int i = 0; i < 100; ++i) {
        float delay = humanizer.getReactionDelay();
        REQUIRE(delay >= 100.0f);
        REQUIRE(delay <= 300.0f);
    }
}

TEST_CASE("Humanizer reaction delay disabled returns zero", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableReactionDelay = false;

    Humanizer humanizer(config);

    float delay = humanizer.getReactionDelay();
    REQUIRE(delay == 0.0f);
}

TEST_CASE("Humanizer reaction delay statistical properties", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableReactionDelay = true;
    config.reactionDelayMean = 160.0f;
    config.reactionDelayStdDev = 25.0f;
    config.reactionDelayMin = 100.0f;
    config.reactionDelayMax = 300.0f;

    Humanizer humanizer(config);

    // Sample 1000 delays and compute mean
    const int N = 1000;
    float sum = 0.0f;
    for (int i = 0; i < N; ++i) {
        sum += humanizer.getReactionDelay();
    }
    float mean = sum / N;

    // Mean should be close to 160ms (within 10ms tolerance for statistical variance)
    REQUIRE_THAT(mean, WithinAbs(160.0f, 10.0f));
}

// =============================================================================
// Humanizer Tremor Tests
// =============================================================================

TEST_CASE("Humanizer tremor disabled returns original movement", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableTremor = false;

    Humanizer humanizer(config);

    cv::Point2f movement{10.0f, 5.0f};
    auto result = humanizer.applyTremor(movement, 0.016f);

    REQUIRE_THAT(result.x, WithinAbs(10.0f, 0.01f));
    REQUIRE_THAT(result.y, WithinAbs(5.0f, 0.01f));
}

TEST_CASE("Humanizer tremor applies sinusoidal jitter", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableTremor = true;
    config.tremorFrequency = 10.0f;   // 10Hz
    config.tremorAmplitude = 0.5f;    // 0.5 pixels

    Humanizer humanizer(config);

    cv::Point2f movement{10.0f, 5.0f};

    // First call (phase = 0)
    auto result1 = humanizer.applyTremor(movement, 0.016f);

    // Jitter should be applied (result != original)
    // At phase=0, sin(0) = 0, so first call might be close to original
    // But subsequent calls should oscillate

    // Second call (phase advanced)
    auto result2 = humanizer.applyTremor(movement, 0.016f);

    // Results should differ due to phase progression
    bool different = (std::abs(result1.x - result2.x) > 0.01f) ||
                     (std::abs(result1.y - result2.y) > 0.01f);
    REQUIRE(different);
}

TEST_CASE("Humanizer tremor amplitude within bounds", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableTremor = true;
    config.tremorFrequency = 10.0f;
    config.tremorAmplitude = 0.5f;

    Humanizer humanizer(config);

    cv::Point2f movement{10.0f, 5.0f};

    // Sample over 100 frames (1.6 seconds at 60fps)
    for (int i = 0; i < 100; ++i) {
        auto result = humanizer.applyTremor(movement, 0.016f);

        // Jitter should be within ±amplitude
        float jitterX = result.x - movement.x;
        float jitterY = result.y - movement.y;

        REQUIRE(std::abs(jitterX) <= config.tremorAmplitude + 0.01f);
        REQUIRE(std::abs(jitterY) <= config.tremorAmplitude + 0.01f);
    }
}

TEST_CASE("Humanizer tremor phase wrapping", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableTremor = true;
    config.tremorFrequency = 10.0f;
    config.tremorAmplitude = 0.5f;

    Humanizer humanizer(config);

    cv::Point2f movement{10.0f, 5.0f};

    // Advance phase beyond 2π multiple times
    // Phase should wrap to prevent overflow
    for (int i = 0; i < 1000; ++i) {
        auto result = humanizer.applyTremor(movement, 0.016f);
        // Should not crash or produce NaN
        REQUIRE(!std::isnan(result.x));
        REQUIRE(!std::isnan(result.y));
    }
}

TEST_CASE("Humanizer tremor reset phase", "[input][humanizer]") {
    Humanizer::Config config;
    config.enableTremor = true;
    config.tremorFrequency = 10.0f;
    config.tremorAmplitude = 0.5f;

    Humanizer humanizer(config);

    cv::Point2f movement{10.0f, 5.0f};

    // Advance phase significantly
    for (int i = 0; i < 10; ++i) {
        humanizer.applyTremor(movement, 0.016f);
    }

    // Get result before reset
    auto before_reset = humanizer.applyTremor(movement, 0.016f);

    // Reset phase
    humanizer.resetTremorPhase();

    // Next result should be different (reset to initial state)
    auto result_after_reset = humanizer.applyTremor(movement, 0.016f);

    // Results should differ significantly because phase was reset
    // After reset: phase = 0 + (freq * dt * 2π) ≈ 1.005 rad
    // Before reset: phase was much higher
    bool reset_worked = std::abs(result_after_reset.x - before_reset.x) > 0.1f ||
                        std::abs(result_after_reset.y - before_reset.y) > 0.1f;
    REQUIRE(reset_worked);
}

TEST_CASE("Humanizer config update", "[input][humanizer]") {
    Humanizer::Config config1;
    config1.enableReactionDelay = true;
    config1.reactionDelayMean = 160.0f;

    Humanizer humanizer(config1);

    // Update config
    Humanizer::Config config2;
    config2.enableReactionDelay = false;
    humanizer.setConfig(config2);

    // Should now return 0 for reaction delay
    float delay = humanizer.getReactionDelay();
    REQUIRE(delay == 0.0f);

    // Verify config was updated
    REQUIRE(humanizer.getConfig().enableReactionDelay == false);
}
