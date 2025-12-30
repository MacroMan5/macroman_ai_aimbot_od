#pragma once
#include <opencv2/core/types.hpp>
#include <cmath>

namespace sunone {

struct BezierCurve {
    cv::Point2f p0, p1, p2, p3;

    /**
     * @brief Calculate point at t [0.0 - 1.0] using Bernstein polynomials
     */
    cv::Point2f at(float t) const {
        // Clamp t
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

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
     * @brief Get total estimated arc length (approximation)
     */
    float length() const {
        // Chord lengths approximation
        float l1 = cv::norm(p1 - p0);
        float l2 = cv::norm(p2 - p1);
        float l3 = cv::norm(p3 - p2);
        return l1 + l2 + l3;
    }
};

} // namespace sunone
