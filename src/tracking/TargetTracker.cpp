#include "TargetTracker.h"
#include <chrono>
#include <algorithm>

namespace macroman {

TargetTracker::TargetTracker(float gracePeriod)
    : gracePeriod_(gracePeriod)
{
    gracePeriodCounters_.fill(0.0f);
}

void TargetTracker::update(const DetectionBatch& batch, float dt) {
    // 1. Data Association: Match detections to existing tracks
    auto result = DataAssociation::matchDetections(db_, batch.observations);

    // Track which targets have been matched
    std::array<bool, TargetDatabase::MAX_TARGETS> matched{};
    matched.fill(false);

    // 2. Update matched tracks
    for (const auto& match : result.matches) {
        updateTrack(match.targetIndex, batch.observations[match.detectionIndex], dt);
        matched[match.targetIndex] = true;
    }

    // 3. Coast unmatched tracks (predict without correction)
    for (size_t i = 0; i < db_.count; ++i) {
        if (!matched[i]) {
            coastTrack(i, dt);
        }
    }

    // 4. Create new tracks for unmatched detections
    for (size_t detIdx : result.unmatchedDetections) {
        createTrack(batch.observations[detIdx]);
    }

    // 5. Remove stale tracks (grace period exceeded)
    removeStaleTracks();
}

AimCommand TargetTracker::getAimCommand(Vec2 crosshair, float fovRadius) const noexcept {
    // Use TargetSelector to find best target
    auto selection = selector_.selectBest(db_, crosshair, fovRadius);

    if (!selection.has_value()) {
        return AimCommand{};  // No target
    }

    // Build AimCommand from selected target
    return AimCommand{
        selection->position,
        db_.confidences[selection->targetIndex],
        selection->hitbox
    };
}

void TargetTracker::createTrack(const Detection& detection) {
    if (db_.count >= TargetDatabase::MAX_TARGETS) {
        return;  // Database full, drop detection
    }

    size_t idx = db_.count++;

    // Calculate bbox center manually
    Vec2 center{
        detection.bbox.x + detection.bbox.width * 0.5f,
        detection.bbox.y + detection.bbox.height * 0.5f
    };

    // Initialize track from detection
    db_.ids[idx] = static_cast<TargetID>(idx);  // Simple ID assignment
    db_.bboxes[idx] = detection.bbox;
    db_.positions[idx] = center;
    db_.velocities[idx] = {0.0f, 0.0f};  // Unknown velocity initially
    db_.confidences[idx] = detection.confidence;
    db_.hitboxTypes[idx] = detection.hitbox;

    // Timestamp in nanoseconds since epoch
    auto now = std::chrono::high_resolution_clock::now();
    db_.lastSeenNs[idx] = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    // Initialize Kalman state (zero velocity assumption)
    db_.kalmanStates[idx] = KalmanState();
    db_.kalmanStates[idx].x = center.x;
    db_.kalmanStates[idx].y = center.y;

    // Reset grace period counter
    gracePeriodCounters_[idx] = 0.0f;
}

void TargetTracker::updateTrack(size_t trackIdx, const Detection& detection, float dt) {
    // Calculate bbox center manually
    Vec2 observation{
        detection.bbox.x + detection.bbox.width * 0.5f,
        detection.bbox.y + detection.bbox.height * 0.5f
    };

    // Update Kalman filter with new observation
    kalman_.update(observation, dt, db_.kalmanStates[trackIdx]);

    // Update position from Kalman estimate
    db_.positions[trackIdx] = {
        db_.kalmanStates[trackIdx].x,
        db_.kalmanStates[trackIdx].y
    };

    // Update velocity from Kalman estimate
    db_.velocities[trackIdx] = {
        db_.kalmanStates[trackIdx].vx,
        db_.kalmanStates[trackIdx].vy
    };

    // Update other fields
    db_.bboxes[trackIdx] = detection.bbox;
    db_.confidences[trackIdx] = detection.confidence;
    db_.hitboxTypes[trackIdx] = detection.hitbox;

    // Timestamp in nanoseconds since epoch
    auto now = std::chrono::high_resolution_clock::now();
    db_.lastSeenNs[trackIdx] = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    // Reset grace period counter (matched = fresh)
    gracePeriodCounters_[trackIdx] = 0.0f;
}

void TargetTracker::coastTrack(size_t trackIdx, float dt) {
    // Predict position using Kalman filter (no correction)
    Vec2 predicted = kalman_.predict(dt, db_.kalmanStates[trackIdx]);
    db_.positions[trackIdx] = predicted;

    // Increment grace period counter
    gracePeriodCounters_[trackIdx] += dt;
}

void TargetTracker::removeStaleTracks() {
    // Remove tracks that exceeded grace period
    // Iterate backwards to avoid index shifting issues
    for (int i = static_cast<int>(db_.count) - 1; i >= 0; --i) {
        if (gracePeriodCounters_[i] > gracePeriod_) {
            // Remove track by swapping with last track
            size_t lastIdx = db_.count - 1;

            if (static_cast<size_t>(i) != lastIdx) {
                // Swap with last track
                db_.ids[i] = db_.ids[lastIdx];
                db_.bboxes[i] = db_.bboxes[lastIdx];
                db_.positions[i] = db_.positions[lastIdx];
                db_.velocities[i] = db_.velocities[lastIdx];
                db_.confidences[i] = db_.confidences[lastIdx];
                db_.hitboxTypes[i] = db_.hitboxTypes[lastIdx];
                db_.lastSeenNs[i] = db_.lastSeenNs[lastIdx];
                db_.kalmanStates[i] = db_.kalmanStates[lastIdx];
                gracePeriodCounters_[i] = gracePeriodCounters_[lastIdx];
            }

            db_.count--;
        }
    }
}

} // namespace macroman
