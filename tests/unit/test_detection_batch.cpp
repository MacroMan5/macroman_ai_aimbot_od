#include <catch2/catch_test_macros.hpp>
#include "core/entities/DetectionBatch.h"

using namespace macroman;

TEST_CASE("DetectionBatch construction", "[entities][detection_batch]") {
    DetectionBatch batch;

    REQUIRE(batch.observations.empty());
    REQUIRE(batch.frameSequence == 0);
    REQUIRE(batch.captureTimeNs == 0);
}

TEST_CASE("DetectionBatch add detections", "[entities][detection_batch]") {
    DetectionBatch batch;
    batch.frameSequence = 42;
    batch.captureTimeNs = 1000000000;  // 1 second in nanoseconds

    Detection det1;
    det1.bbox = {10.0f, 20.0f, 50.0f, 60.0f};
    det1.confidence = 0.9f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;

    Detection det2;
    det2.bbox = {100.0f, 200.0f, 50.0f, 60.0f};
    det2.confidence = 0.85f;
    det2.classId = 1;
    det2.hitbox = HitboxType::Chest;

    batch.observations.push_back(det1);
    batch.observations.push_back(det2);

    REQUIRE(batch.observations.size() == 2);
    REQUIRE(batch.observations[0].confidence == 0.9f);
    REQUIRE(batch.observations[1].hitbox == HitboxType::Chest);
}

TEST_CASE("DetectionBatch capacity limit", "[entities][detection_batch]") {
    DetectionBatch batch;

    // Fill to capacity
    for (size_t i = 0; i < DetectionBatch::MAX_DETECTIONS; ++i) {
        Detection det;
        det.bbox = {0.0f, 0.0f, 1.0f, 1.0f};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Body;
        batch.observations.push_back(det);
    }

    REQUIRE(batch.observations.size() == DetectionBatch::MAX_DETECTIONS);
    REQUIRE(batch.observations.capacity() == DetectionBatch::MAX_DETECTIONS);
}
