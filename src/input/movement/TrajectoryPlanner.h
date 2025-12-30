#pragma once

#include "../../core/entities/MouseMovement.h"
#include "BezierCurve.h"
#include "OneEuroFilter.h"
#include <opencv2/core/types.hpp>
#include <random>

namespace sunone {

/**
 * @struct TrajectoryConfig
 * @brief Configuration parameters for mouse movement generation.
 */
struct TrajectoryConfig {
    // Mouse Sensitivity Factor
    // 1.0 means 1 pixel detected movement = 1 mouse count (at base FOV)
    float sensitivity = 1.0f;
    
    // Horizontal Field of View in degrees
    float fovX = 70.0f;
    // Vertical Field of View in degrees
    float fovY = 50.0f;
    
    int screenWidth = 1920;
    int screenHeight = 1080;

    // --- Smoothing Settings ---
    bool smoothingEnabled = true;
    
    // 1 Euro Filter Parameters
    // minCutoff: Minimize jitter (1.0 is a good start, lower is smoother)
    float minCutoff = 0.5f;
    // beta: Speed coefficient (0.007 is good, higher is faster response)
    float beta = 0.05f;

    // --- Legacy Simple Smoothing ---
    // Interpolation factor [0.0 - 1.0]
    // Used when Bezier is DISABLED and OneEuro is NOT used (fallback).
    float easingFactor = 0.5f;
    
    // Speed clamping (in mouse counts per update)
    float minSpeed = 1.0f;
    float maxSpeed = 10.0f;

    // --- Bezier Settings (Replaces Wind Mouse) ---
    bool bezierEnabled = true;
    
    // 0.0 = Straight line, 1.0 = Very curved
    float bezierCurvature = 0.4f; 
    
    // Time (in seconds) to complete a full flick movement
    float minPathDuration = 0.2f;
    float maxPathDuration = 0.5f;

    // --- Legacy Wind Mouse (Fallback) ---
    bool windMouseEnabled = false;
    float windGravity = 9.0f;
    float windWind = 3.0f;
};

/**
 * @class TrajectoryPlanner
 * @brief Calculates the mouse input required to move from A to B.
 *
 * Now supports stateful Bezier paths.
 */
class TrajectoryPlanner {
public:
    explicit TrajectoryPlanner(const TrajectoryConfig& config = {});

    /**
     * @brief Plans a single movement step towards the target.
     *
     * @param current The current reticle position (usually screen center).
     * @param target The destination pixel coordinates.
     * @return MouseMovement containing dx, dy values for the input driver.
     */
    MouseMovement plan(const cv::Point2f& current,
                      const cv::Point2f& target);

    /**
     * @brief Plans a movement that blends the current target position with a predicted future position.
     *
     * @param current The current reticle position.
     * @param predicted The predicted future position of the target.
     * @param confidence A weight [0.0 - 1.0] used to interpolate between the
     *                   raw detection (0.0) and the full prediction (1.0).
     */
    MouseMovement planWithPrediction(const cv::Point2f& current,
                                     const cv::Point2f& predicted,
                                     float confidence);

    /**
     * @brief Resets the current path state.
     * Call this when the aim key is released or target is lost.
     */
    void reset();

    void setConfig(const TrajectoryConfig& config) { 
        config_ = config; 
        filterX_.updateParams(config_.minCutoff, config_.beta);
        filterY_.updateParams(config_.minCutoff, config_.beta);
    }
    const TrajectoryConfig& getConfig() const { return config_; }

private:
    /**
     * @brief Converts screen pixel distance to mouse counts.
     */
    MouseMovement screenToMouse(float dx, float dy) const;

    // Legacy logic
    MouseMovement applySmoothing(const MouseMovement& raw) const;
    MouseMovement applyWindMouse(const MouseMovement& raw) const;

    // Bezier logic
    MouseMovement advanceBezier(const cv::Point2f& current, const cv::Point2f& target, float dt);
    void generateNewCurve(const cv::Point2f& start, const cv::Point2f& end);
    void updateCurveEnd(const cv::Point2f& current, const cv::Point2f& newEnd);
    
    TrajectoryConfig config_;
    
    // State
    BezierCurve activeCurve_;
    float t_ = 0.0f;
    float stepSize_ = 0.01f; // Calculated based on duration
    bool hasActivePath_ = false;
    cv::Point2f lastTarget_ = {0, 0};
    
    // Time & Velocity tracking
    std::chrono::steady_clock::time_point lastTime_;
    cv::Point2f currentVelocity_ = {0, 0}; // pixels/sec

    // Filters
    OneEuroFilter filterX_;
    OneEuroFilter filterY_;

    // Random engine for curve generation
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace sunone