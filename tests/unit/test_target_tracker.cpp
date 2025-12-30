#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "tracking/TargetTracker.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("TargetTracker initializes empty", "[tracking][tracker]") {
    TargetTracker tracker;

    Vec2 crosshair{960.0f, 540.0f};
    auto cmd = tracker.getAimCommand(crosshair, 100.0f);

    REQUIRE_FALSE(cmd.hasTarget);
}

TEST_CASE("TargetTracker creates new track from detection", "[tracking][tracker]") {
    TargetTracker tracker;

    DetectionBatch batch;
    Detection det;
    det.bbox = {100.0f, 100.0f, 50.0f, 80.0f};
    det.confidence = 0.9f;
    det.classId = 0;
    det.hitbox = HitboxType::Head;
    batch.observations.push_back(det);

    auto now = std::chrono::high_resolution_clock::now();
    batch.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch, 0.016f);

    Vec2 crosshair{125.0f, 140.0f};  // Near detection center
    auto cmd = tracker.getAimCommand(crosshair, 200.0f);

    REQUIRE(cmd.hasTarget);
    REQUIRE(cmd.confidence > 0.8f);
    REQUIRE(cmd.hitbox == HitboxType::Head);
}

TEST_CASE("TargetTracker updates existing track", "[tracking][tracker]") {
    TargetTracker tracker;

    Vec2 crosshair{125.0f, 140.0f};  // Near the target positions

    // Frame 1: Initial detection
    DetectionBatch batch1;
    Detection det1;
    det1.bbox = {100.0f, 100.0f, 50.0f, 80.0f};
    det1.confidence = 0.9f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;
    batch1.observations.push_back(det1);

    auto now = std::chrono::high_resolution_clock::now();
    batch1.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch1, 0.016f);

    // Frame 2: Target moved right
    DetectionBatch batch2;
    Detection det2;
    det2.bbox = {110.0f, 100.0f, 50.0f, 80.0f};
    det2.confidence = 0.9f;
    det2.classId = 0;
    det2.hitbox = HitboxType::Head;
    batch2.observations.push_back(det2);

    batch2.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch2, 0.016f);

    // Should still have one track with updated position
    auto cmd = tracker.getAimCommand(crosshair, 100.0f);
    REQUIRE(cmd.hasTarget);
    REQUIRE(cmd.targetPosition.x > 100.0f);  // Position updated
}

TEST_CASE("TargetTracker maintains tracks during grace period", "[tracking][tracker]") {
    TargetTracker tracker;

    Vec2 crosshair{125.0f, 140.0f};  // Near the target position

    // Frame 1: Detection
    DetectionBatch batch1;
    Detection det1;
    det1.bbox = {100.0f, 100.0f, 50.0f, 80.0f};
    det1.confidence = 0.9f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;
    batch1.observations.push_back(det1);

    auto now = std::chrono::high_resolution_clock::now();
    batch1.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch1, 0.016f);

    // Frame 2: No detection (occlusion, detection failure)
    DetectionBatch batch2;
    batch2.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch2, 0.016f);

    // Track should still exist during grace period (coasting)
    auto cmd = tracker.getAimCommand(crosshair, 100.0f);
    REQUIRE(cmd.hasTarget);  // Still tracking via prediction
}

TEST_CASE("TargetTracker removes stale tracks after grace period", "[tracking][tracker]") {
    TargetTracker tracker(0.032f);  // 32ms grace period (2 frames)

    Vec2 crosshair{960.0f, 540.0f};

    // Frame 1: Detection
    DetectionBatch batch1;
    Detection det1;
    det1.bbox = {100.0f, 100.0f, 50.0f, 80.0f};
    det1.confidence = 0.9f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;
    batch1.observations.push_back(det1);

    auto now = std::chrono::high_resolution_clock::now();
    batch1.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch1, 0.016f);

    // Frames 2-4: No detections (exceeds grace period)
    for (int i = 0; i < 3; ++i) {
        DetectionBatch emptyBatch;
        emptyBatch.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();

        tracker.update(emptyBatch, 0.016f);
    }

    // Track should be removed after grace period
    auto cmd = tracker.getAimCommand(crosshair, 500.0f);
    REQUIRE_FALSE(cmd.hasTarget);
}

TEST_CASE("TargetTracker selects best target by FOV and hitbox", "[tracking][tracker]") {
    TargetTracker tracker;

    Vec2 crosshair{960.0f, 540.0f};

    // Create multiple detections
    DetectionBatch batch;

    Detection det1;
    det1.bbox = {850.0f, 500.0f, 50.0f, 80.0f};  // Body, far
    det1.confidence = 0.9f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Body;
    batch.observations.push_back(det1);

    Detection det2;
    det2.bbox = {900.0f, 500.0f, 50.0f, 80.0f};  // Head, closer
    det2.confidence = 0.9f;
    det2.classId = 0;
    det2.hitbox = HitboxType::Head;
    batch.observations.push_back(det2);

    auto now = std::chrono::high_resolution_clock::now();
    batch.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    tracker.update(batch, 0.016f);

    // Should select Head target (higher priority)
    auto cmd = tracker.getAimCommand(crosshair, 200.0f);
    REQUIRE(cmd.hasTarget);
    REQUIRE(cmd.hitbox == HitboxType::Head);
}
