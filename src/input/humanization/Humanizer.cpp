#include "Humanizer.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace macroman {

Humanizer::Humanizer(const Config& config)
    : config_(config)
    , rng_(std::random_device{}())
    , reactionDist_(config.reactionDelayMean, config.reactionDelayStdDev)
{
}

float Humanizer::getReactionDelay() {
    if (!config_.enableReactionDelay) {
        return 0.0f;
    }

    // Sample from Normal Distribution N(μ=160ms, σ=25ms)
    float delay = reactionDist_(rng_);

    // Clamp to human-possible range [100ms, 300ms]
    delay = std::clamp(delay, config_.reactionDelayMin, config_.reactionDelayMax);

    return delay;
}

Vec2 Humanizer::applyTremor(const Vec2& movement, float dt) {
    if (!config_.enableTremor) {
        return movement;
    }

    // Update tremor phase based on frequency and time delta
    tremorPhase_ += config_.tremorFrequency * dt * 2.0f * static_cast<float>(M_PI);

    // Wrap phase to [0, 2π] to prevent float overflow
    if (tremorPhase_ > 2.0f * M_PI) {
        tremorPhase_ -= 2.0f * static_cast<float>(M_PI);
    }

    // Apply sinusoidal tremor
    float jitterX = config_.tremorAmplitude * std::sin(tremorPhase_);
    float jitterY = config_.tremorAmplitude * std::sin(tremorPhase_ * 1.3f);  // Different frequency for Y

    return {movement.x + jitterX, movement.y + jitterY};
}

void Humanizer::resetTremorPhase() {
    tremorPhase_ = 0.0f;
}

} // namespace macroman
