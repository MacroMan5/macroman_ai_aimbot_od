#pragma once

#include "core/entities/TargetDatabase.h"
#include "core/entities/DetectionBatch.h"
#include "core/entities/AimCommand.h"
#include "tracking/DataAssociation.h"
#include "tracking/KalmanPredictor.h"
#include "tracking/TargetSelector.h"

namespace macroman {

/**
 * @brief High-level target tracking system with track maintenance
 *
 * Integrates DataAssociation, KalmanPredictor, and TargetSelector to:
 * - Associate detections with existing tracks
 * - Update Kalman filters for matched tracks
 * - Coast unmatched tracks for grace period (predict without correction)
 * - Remove stale tracks after grace period
 * - Select best target and produce AimCommand
 *
 * Track Maintenance Strategy:
 * - Matched tracks: Update position and Kalman state (normal tracking)
 * - Unmatched tracks: Increment gracePeriodCounter (coasting)
 * - Stale tracks (gracePeriodCounter > threshold): Remove from database
 */
class TargetTracker {
public:
    /**
     * @brief Construct tracker with configurable grace period
     * @param gracePeriod Time in seconds to coast unmatched tracks before removal (default: 100ms)
     */
    explicit TargetTracker(float gracePeriod = 0.1f);

    /**
     * @brief Update tracker with new detection batch
     * @param batch Detection observations from current frame
     * @param dt Time delta since last update (seconds)
     *
     * Process:
     * 1. Associate detections with existing tracks (IoU matching)
     * 2. Update matched tracks (Kalman correction + reset grace counter)
     * 3. Coast unmatched tracks (Kalman prediction only + increment grace counter)
     * 4. Create new tracks for unmatched detections
     * 5. Remove stale tracks (grace period exceeded)
     */
    void update(const DetectionBatch& batch, float dt);

    /**
     * @brief Get aim command for Input thread
     * @param crosshair Crosshair position (screen center)
     * @param fovRadius FOV radius in pixels
     * @return AimCommand with best target, or no target if none valid
     */
    [[nodiscard]] AimCommand getAimCommand(Vec2 crosshair, float fovRadius) const noexcept;

    /**
     * @brief Get current target database (for debugging/visualization)
     */
    [[nodiscard]] const TargetDatabase& getDatabase() const noexcept { return db_; }

private:
    TargetDatabase db_;                  // Structure-of-Arrays target storage
    DataAssociation dataAssoc_;          // Greedy IoU matching
    KalmanPredictor kalman_;             // Stateless Kalman filter
    TargetSelector selector_;            // Target selection logic

    float gracePeriod_;                  // Grace period in seconds (default: 0.1s)
    std::array<float, TargetDatabase::MAX_TARGETS> gracePeriodCounters_{};  // Time since last match

    /**
     * @brief Create new track from detection
     */
    void createTrack(const Detection& detection);

    /**
     * @brief Update existing track with detection
     */
    void updateTrack(size_t trackIdx, const Detection& detection, float dt);

    /**
     * @brief Coast track without detection (prediction only)
     */
    void coastTrack(size_t trackIdx, float dt);

    /**
     * @brief Remove stale tracks (grace period exceeded)
     */
    void removeStaleTracks();
};

} // namespace macroman
