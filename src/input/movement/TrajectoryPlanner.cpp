#include "TrajectoryPlanner.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace macroman {

TrajectoryPlanner::TrajectoryPlanner(const TrajectoryConfig& config)
    : config_(config) {
    lastTime_ = std::chrono::steady_clock::now();
    filterX_.updateParams(config_.minCutoff, config_.beta);
    filterY_.updateParams(config_.minCutoff, config_.beta);
}

void TrajectoryPlanner::reset() {
    hasActivePath_ = false;
    t_ = 0.0f;
    lastTarget_ = {0, 0};
    currentVelocity_ = {0, 0};
    lastTime_ = std::chrono::steady_clock::now();
    filterX_.reset();
    filterY_.reset();
}

MouseMovement TrajectoryPlanner::plan(const cv::Point2f& current,
                                      const cv::Point2f& target) {
    // Update time delta
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;
    
    // Clamp dt to avoid huge jumps if thread slept (max 50ms)
    if (dt > 0.05f) dt = 0.05f;

    if (config_.bezierEnabled) {
        return advanceBezier(current, target, dt);
    }

    // Legacy / Fallback logic
    float dx = target.x - current.x;
    float dy = target.y - current.y;

    auto movement = screenToMouse(dx, dy);

    if (config_.windMouseEnabled) {
        movement = applyWindMouse(movement);
    } else if (config_.smoothingEnabled) {
        // Use 1 Euro Filter for superior smoothing
        // We filter the Delta, which is a bit unusual (usually you filter position),
        // but for relative mouse movement, filtering the delta stream works to smooth out spikes.
        // Better: Filter the TARGET position before calculating delta? 
        // No, plan() takes current/target. Let's filter the OUTPUT movement.
        
        if (dt > 0.0001f) {
            float smoothDx = filterX_.filter(static_cast<float>(movement.dx), dt);
            float smoothDy = filterY_.filter(static_cast<float>(movement.dy), dt);
            movement.dx = static_cast<int>(std::round(smoothDx));
            movement.dy = static_cast<int>(std::round(smoothDy));
        } else {
             movement = applySmoothing(movement); // Fallback if dt is zero
        }
    }

    return movement;
}

MouseMovement TrajectoryPlanner::planWithPrediction(
    const cv::Point2f& current,
    const cv::Point2f& predicted,
    float confidence) {

    // Blend between current and predicted based on confidence
    cv::Point2f target;
    target.x = current.x + (predicted.x - current.x) * confidence;
    target.y = current.y + (predicted.y - current.y) * confidence;

    return plan(current, target);
}

// --------------------------------------------------------------------------
// Bezier Implementation
// --------------------------------------------------------------------------

MouseMovement TrajectoryPlanner::advanceBezier(const cv::Point2f& current,
                                               const cv::Point2f& target,
                                               float dt) {
    // 1. Detect if we need a new curve
    float distToLastTarget = cv::norm(target - lastTarget_);
    bool isNewTarget = distToLastTarget > 50.0f && hasActivePath_;

    if (!hasActivePath_ || isNewTarget) {
        generateNewCurve(current, target);
    } else {
        // Dynamic Dragging: Update endpoint and P2
        updateCurveEnd(current, target);
    }

    lastTarget_ = target;

    // 2. Advance time (t)
    // Step size is now derived from dt and duration
    // stepSize_ stored 1/duration. So t += dt * stepSize_
    t_ += dt * stepSize_;
    
    if (t_ > 1.0f) t_ = 1.0f;

    // 3. Calculate next position on curve
    cv::Point2f nextPos = activeCurve_.at(t_);

    // 4. Calculate delta needed from CURRENT actual position
    float dx = nextPos.x - current.x;
    float dy = nextPos.y - current.y;
    
    // Estimate velocity for next frame (pixels/sec)
    if (dt > 0.0001f) {
        currentVelocity_ = cv::Point2f(dx / dt, dy / dt);
    }

    // Check if we reached the end
    if (t_ >= 1.0f) {
        // Snap to target if very close
        if (std::abs(dx) < 1.0f && std::abs(dy) < 1.0f) {
            return MouseMovement(0, 0);
        }
    }

    return screenToMouse(dx, dy);
}

void TrajectoryPlanner::generateNewCurve(const cv::Point2f& start, const cv::Point2f& end) {
    activeCurve_.p0 = start;
    activeCurve_.p3 = end;

    float dist = cv::norm(end - start);
    
    // Randomizers
    std::uniform_real_distribution<float> curvDist(0.1f, config_.bezierCurvature);
    std::uniform_real_distribution<float> timeDist(config_.minPathDuration, config_.maxPathDuration);
    std::uniform_int_distribution<int> sideDist(0, 1);

    // -----------------------------------------------------------------
    // P1: Initial Control Point (Departure)
    // Should align with current velocity to be smooth, or random if stationary
    // -----------------------------------------------------------------
    cv::Point2f p1Dir;
    float velocityMag = cv::norm(currentVelocity_);
    
    // Heuristic: If moving fast enough (> 100px/s), use velocity momentum
    if (velocityMag > 100.0f) {
        p1Dir = currentVelocity_ / velocityMag;
    } else {
        // Otherwise, use direction to target + random noise
        cv::Point2f dir = end - start;
        float dirLen = cv::norm(dir);
        if (dirLen > 0.001f) p1Dir = dir / dirLen;
        
        // Add perpendicular noise
        cv::Point2f perp(-p1Dir.y, p1Dir.x);
        float noise = (sideDist(rng_) ? 1.0f : -1.0f) * curvDist(rng_);
        p1Dir += perp * noise;
    }
    
    // Place P1 at ~25-40% of distance
    float p1Dist = dist * 0.33f; 
    activeCurve_.p1 = start + p1Dir * p1Dist;

    // -----------------------------------------------------------------
    // P2: Final Control Point (Arrival)
    // Should align with approach to target to avoid overshooting
    // -----------------------------------------------------------------
    cv::Point2f dirToEnd = end - start;
    float dirLen = cv::norm(dirToEnd);
    if (dirLen > 0.001f) dirToEnd /= dirLen;
    
    // Add some random arc to the approach
    cv::Point2f perp(-dirToEnd.y, dirToEnd.x);
    float noise2 = (sideDist(rng_) ? 1.0f : -1.0f) * curvDist(rng_);
    
    // P2 is "behind" P3, so we subtract from P3
    // Standard cubic bezier heuristic: P2 is at ~66% of way, or P3 - vector
    activeCurve_.p2 = end - (dirToEnd + perp * noise2) * (dist * 0.33f);


    // -----------------------------------------------------------------
    // Duration / Speed
    // -----------------------------------------------------------------
    float duration = timeDist(rng_);
    if (duration < 0.1f) duration = 0.1f;
    
    // Store 1/duration so we can just do t += dt * stepSize_
    stepSize_ = 1.0f / duration;
    
    t_ = 0.0f;
    hasActivePath_ = true;
}

void TrajectoryPlanner::updateCurveEnd(const cv::Point2f& current, const cv::Point2f& newEnd) {
    // "Drag" the curve: Move P3 to new location
    // Recalculate P2 to maintain the "approach angle" relative to the new P3
    
    // 1. Update Endpoint
    activeCurve_.p3 = newEnd;
    
    // 2. Recalculate P2
    // We want P2 to maintain its relative "shape" with respect to P3 and P0
    // Simple heuristic: P2 is usually P3 - (Direction * scale)
    // We can just keep the original "offset" of P2 relative to P3, but rotated?
    // Simpler: Just re-interpolate P2 between P0 and P3
    
    cv::Point2f dir = newEnd - activeCurve_.p0;
    float dist = cv::norm(dir);
    if (dist > 0.001f) dir /= dist;
    
    // Preserve the "curvature" factor we picked during generation
    // But for tracking, stability is better.
    // Let's just linearly interpolate P2 towards the new ideal P2 position
    // New Ideal P2 = newEnd - (dir * 0.33 * dist)
    
    cv::Point2f idealP2 = newEnd - (dir * (dist * 0.33f));
    
    // Blend current P2 with Ideal P2 to avoid snaps
    activeCurve_.p2 = activeCurve_.p2 * 0.8f + idealP2 * 0.2f;
}

// --------------------------------------------------------------------------
// Helpers / Legacy
// --------------------------------------------------------------------------

MouseMovement TrajectoryPlanner::screenToMouse(float dx, float dy) const {
    // Convert screen pixels to mouse movement
    // Based on FOV and sensitivity

    // Avoid division by zero
    float sw = static_cast<float>(config_.screenWidth);
    float sh = static_cast<float>(config_.screenHeight);
    if (sw <= 0) sw = 1920;
    if (sh <= 0) sh = 1080;

    float mouseX = dx * config_.sensitivity * (config_.fovX / sw);
    float mouseY = dy * config_.sensitivity * (config_.fovY / sh);

    return MouseMovement(
        static_cast<int>(std::round(mouseX)),
        static_cast<int>(std::round(mouseY))
    );
}

MouseMovement TrajectoryPlanner::applySmoothing(const MouseMovement& raw) const {
    // Can't implement OneEuro here because this method is const and OneEuro has state.
    // However, the main plan() method handles the 1 Euro logic now if we integrate it there.
    // For legacy compatibility, this method keeps the simple lerp.
    // See plan() for OneEuro integration.
    
    float magnitude = raw.magnitude();

    // Clamp speed
    float speed = std::clamp(magnitude, config_.minSpeed, config_.maxSpeed);

    // Apply easing
    float factor = config_.easingFactor;
    if (magnitude > 0) {
        factor = std::min(factor, speed / magnitude);
    }

    return MouseMovement(
        static_cast<int>(raw.dx * factor),
        static_cast<int>(raw.dy * factor)
    );
}

MouseMovement TrajectoryPlanner::applyWindMouse(const MouseMovement& raw) const {
    // Simplified wind mouse - adds natural curve
    float gravity = config_.windGravity;
    float wind = config_.windWind;

    // Random wind effect
    float windX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * wind;
    float windY = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * wind;

    // Gravity pulls toward target
    float magnitude = raw.magnitude();
    if (magnitude > 0) {
        float gravX = raw.dx / magnitude * gravity;
        float gravY = raw.dy / magnitude * gravity;

        return MouseMovement(
            static_cast<int>(raw.dx * 0.5f + gravX + windX),
            static_cast<int>(raw.dy * 0.5f + gravY + windY)
        );
    }

    return raw;
}

} // namespace macroman
