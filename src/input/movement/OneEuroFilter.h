#pragma once
#include <cmath>
#include <chrono>

namespace macroman {

class LowPassFilter {
public:
    LowPassFilter() : y(0), s(0), initialized(false) {}

    float filter(float value, float alpha) {
        if (!initialized) {
            s = value;
            initialized = true;
        } else {
            s = alpha * value + (1.0f - alpha) * s;
        }
        y = value;
        return s;
    }

    float lastValue() const { return y; }
    bool isInitialized() const { return initialized; }
    void reset() { initialized = false; }

private:
    float y; // last raw value
    float s; // last filtered value
    bool initialized;
};

/**
 * @brief 1 Euro Filter implementation.
 * Adaptive low-pass filter that minimizes jitter at low speeds 
 * while maintaining low latency at high speeds.
 * Ref: http://cristal.univ-lille.fr/~casiez/1euro/
 */
class OneEuroFilter {
public:
    /**
     * @param minCutoff Minimum cutoff frequency (Hz) to minimize jitter. Lower = smoother but more lag.
     * @param beta Speed coefficient. Higher = more responsive (less lag) at high speeds.
     * @param dCutoff Cutoff frequency for the derivative (Hz). Usually 1.0.
     */
    OneEuroFilter(float minCutoff = 1.0f, float beta = 0.0f, float dCutoff = 1.0f)
        : minCutoff_(minCutoff), beta_(beta), dCutoff_(dCutoff) {}

    float filter(float value, float dt) {
        // Estimate derivative (velocity)
        float dx = 0.0f;
        if (xFilter_.isInitialized()) {
            dx = (value - xFilter_.lastValue()) / dt;
        }
        float edx = dxFilter_.filter(dx, alpha(dt, dCutoff_));

        // Use derivative to calculate cutoff frequency
        float cutoff = minCutoff_ + beta_ * std::abs(edx);

        // Filter value
        return xFilter_.filter(value, alpha(dt, cutoff));
    }

    void reset() {
        xFilter_.reset();
        dxFilter_.reset();
    }

    void updateParams(float minCutoff, float beta) {
        minCutoff_ = minCutoff;
        beta_ = beta;
    }

private:
    float alpha(float dt, float cutoff) {
        float tau = 1.0f / (2.0f * 3.14159265f * cutoff);
        return 1.0f / (1.0f + tau / dt);
    }

    float minCutoff_;
    float beta_;
    float dCutoff_;
    LowPassFilter xFilter_;
    LowPassFilter dxFilter_;
};

} // namespace macroman
