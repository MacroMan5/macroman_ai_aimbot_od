#pragma once

#include <opencv2/core/types.hpp>
#include <random>
#include <chrono>

namespace macroman {

/**
 * @brief Applies human-like imperfections to mouse movement for behavioral humanization.
 *
 * Implements two key humanization techniques from Design Doc Section 10.1:
 * 1. Reaction Delay Manager: Simulates human reaction time (μ=160ms, σ=25ms)
 * 2. Physiological Tremor: Adds 10Hz sinusoidal micro-jitter (0.5px amplitude)
 *
 * Philosophy: "Don't be invisible; be indistinguishable from a human."
 * Professional players exhibit these natural imperfections, making aim assistance
 * indistinguishable from skilled human play.
 */
class Humanizer {
public:
    struct Config {
        // Reaction Delay Settings
        bool enableReactionDelay = true;
        float reactionDelayMean = 160.0f;    // μ = 160ms (trained gamer average)
        float reactionDelayStdDev = 25.0f;   // σ = 25ms (natural variation)
        float reactionDelayMin = 100.0f;     // Minimum human-possible reaction
        float reactionDelayMax = 300.0f;     // Maximum bounded delay

        // Physiological Tremor Settings
        bool enableTremor = true;
        float tremorFrequency = 10.0f;       // 10Hz (physiological hand tremor)
        float tremorAmplitude = 0.5f;        // 0.5 pixels (subtle but measurable)
    };

    explicit Humanizer(const Config& config = {});

    /**
     * @brief Calculates reaction delay for a new target acquisition.
     *
     * Samples from Normal Distribution N(μ=160ms, σ=25ms), clamped to [100ms, 300ms].
     * Call this when the tracking thread switches to a new target.
     *
     * @return Reaction delay in milliseconds
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
    cv::Point2f applyTremor(const cv::Point2f& movement, float dt);

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
