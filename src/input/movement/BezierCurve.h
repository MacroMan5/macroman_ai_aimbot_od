#pragma once
#include <opencv2/core/types.hpp>
#include <cmath>

namespace macroman {

struct BezierCurve {
    cv::Point2f p0, p1, p2, p3;
    float overshootFactor = 0.15f;  // 15% overshoot for humanization

    /**
     * @brief Calculate point at t [0.0 - 1.15] using Bernstein polynomials with overshoot.
     *
     * Phase 1 (t ∈ [0.0, 1.0]): Normal Bezier curve toward target (p3)
     * Phase 2 (t ∈ [1.0, 1.15]): Overshoot past target, then correct back
     *
     * This mimics human flick shots where professional players overshoot 5-20%
     * and then micro-correct back to the target.
     *
     * @param t Progress along curve. Values > 1.15 are clamped to 1.15.
     */
    cv::Point2f at(float t) const {
        // Clamp lower bound
        if (t < 0.0f) t = 0.0f;

        // Phase 1: Normal Bezier (t ∈ [0.0, 1.0])
        if (t <= 1.0f) {
            return evaluateBezier(t);
        }

        // Phase 2: Overshoot & Correction (t ∈ [1.0, 1.15])
        if (t <= 1.15f) {
            return evaluateOvershoot(t);
        }

        // Clamp upper bound - settle at target
        return p3;
    }

    /**
     * @brief Get total estimated arc length (approximation)
     */
    float length() const {
        // Chord lengths approximation
        float l1 = cv::norm(p1 - p0);
        float l2 = cv::norm(p2 - p1);
        float l3 = cv::norm(p3 - p2);
        return l1 + l2 + l3;
    }

private:
    /**
     * @brief Standard cubic Bezier evaluation using Bernstein polynomials
     */
    cv::Point2f evaluateBezier(float t) const {
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;

        cv::Point2f p;
        p.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
        p.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
        return p;
    }

    /**
     * @brief Overshoot phase: goes 15% past target, then corrects back
     *
     * At t=1.0: at target (p3)
     * At t=1.075: at overshoot peak (p3 + 15% in direction of movement)
     * At t=1.15: back at target (p3) - correction complete
     */
    cv::Point2f evaluateOvershoot(float t) const {
        // Calculate overshoot direction (direction from p2 to p3)
        cv::Point2f direction = p3 - p2;
        float dirLength = cv::norm(direction);

        if (dirLength < 0.001f) {
            // No movement, just return target
            return p3;
        }

        direction /= dirLength;  // Normalize

        // Calculate overshoot target (15% past p3)
        cv::Point2f overshootTarget = p3 + direction * (dirLength * overshootFactor);

        // Map t ∈ [1.0, 1.15] to overshoot progress
        // t=1.0 → 0.0 (at p3)
        // t=1.075 → 1.0 (at peak overshoot)
        // t=1.15 → 0.0 (back at p3)

        float overshootT = (t - 1.0f) / 0.15f;  // Normalize to [0.0, 1.0]

        // Use parabolic ease-out for smooth correction
        // Peak at t=0.5 (which is t=1.075 in original scale)
        float overshootAmount = 1.0f - std::pow(2.0f * overshootT - 1.0f, 2.0f);

        // Interpolate between target and overshoot
        return p3 + (overshootTarget - p3) * overshootAmount;
    }
};

} // namespace macroman
