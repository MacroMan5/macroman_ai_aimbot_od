#pragma once

#include "core/entities/MathTypes.h"
#include <random>
#include <chrono>

namespace macroman {

/**
 * @brief Applies human-like imperfections to mouse movement for behavioral humanization.
 *
 * Implements two key humanization techniques:
 * 1. Processing Delay: Simulates realistic processing latency (μ=12ms, σ=5ms)
 *    - Represents actual computer processing time (capture, detection, tracking)
 *    - 6-20x faster than human reaction time (~150-250ms)
 * 2. Physiological Tremor: Adds 10Hz sinusoidal micro-jitter (0.5px amplitude)
 *
 * Philosophy: "Superhuman baseline + realistic imperfections"
 * System reacts much faster than humans but maintains natural movement patterns
 * for behavioral safety.
 */
class Humanizer {
public:
    struct Config {
        // Processing Delay Settings (Superhuman + Variance)
        bool enableReactionDelay = true;
        float reactionDelayMean = 12.0f;     // μ = 12ms (typical processing delay)
        float reactionDelayStdDev = 5.0f;    // σ = 5ms (network/system variance)
        float reactionDelayMin = 5.0f;       // Minimum: 5ms (fast processing)
        float reactionDelayMax = 25.0f;      // Maximum: 25ms (bounded delay)

        // Physiological Tremor Settings
        bool enableTremor = true;
        float tremorFrequency = 10.0f;       // 10Hz (physiological hand tremor)
        float tremorAmplitude = 0.5f;        // 0.5 pixels (subtle but measurable)
    };

    explicit Humanizer(const Config& config = {});

    /**
     * @brief Calculates processing delay for a new target acquisition.
     *
     * Samples from Normal Distribution N(μ=12ms, σ=5ms), clamped to [5ms, 25ms].
     * Represents realistic computer processing time (capture, detection, tracking).
     * Call this when the tracking thread switches to a new target.
     *
     * Result: 6-20x faster than human reaction time (~150-250ms)
     *
     * @return Processing delay in milliseconds
     */
    float getReactionDelay();

    /**
     * @brief Applies physiological tremor to movement delta.
     *
     * Adds 10Hz sinusoidal micro-jitter in both X and Y axes.
     * The Y axis uses a different phase (1.3x frequency) for organic variation.
     *
     * @param movement Original mouse movement vector
     * @param dt Time delta since last update (for tremor phase calculation)
     * @return Movement with tremor applied
     */
    Vec2 applyTremor(const Vec2& movement, float dt);

    /**
     * @brief Resets tremor phase (call when aim key is released or target lost)
     */
    void resetTremorPhase();

    void setConfig(const Config& config) { config_ = config; }
    const Config& getConfig() const { return config_; }

private:
    Config config_;

    // Reaction Delay
    std::mt19937 rng_;
    std::normal_distribution<float> reactionDist_;

    // Physiological Tremor
    float tremorPhase_ = 0.0f;  // Current phase in radians
};

} // namespace macroman
