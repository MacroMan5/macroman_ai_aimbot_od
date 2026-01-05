#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/DataAssociation.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("IoU calculation", "[tracking][data_association]") {
    SECTION("Perfect overlap (IoU = 1.0)") {
        BBox a{10.0f, 20.0f, 50.0f, 60.0f};
        BBox b{10.0f, 20.0f, 50.0f, 60.0f};

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(1.0f, 0.001f));
    }

    SECTION("No overlap (IoU = 0.0)") {
        BBox a{10.0f, 20.0f, 50.0f, 60.0f};
        BBox b{100.0f, 200.0f, 50.0f, 60.0f};

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Partial overlap") {
        BBox a{0.0f, 0.0f, 10.0f, 10.0f};    // Area = 100
        BBox b{5.0f, 5.0f, 10.0f, 10.0f};    // Area = 100
        // Intersection: (5,5) to (10,10) = 5x5 = 25
        // Union: 100 + 100 - 25 = 175
        // IoU: 25 / 175 ≈ 0.143

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(0.143f, 0.01f));
    }
}

TEST_CASE("Data association greedy matching", "[tracking][data_association]") {
    TargetDatabase db;

    // Add 2 existing targets
    TargetID id1{1};
    TargetID id2{2};
    db.addTarget(id1, {100.0f, 100.0f}, {}, {90.0f, 90.0f, 20.0f, 20.0f}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {190.0f, 190.0f, 20.0f, 20.0f}, 0.85f, HitboxType::Chest, 0);

    // Create 3 new detections
    std::vector<Detection> detections;
    Detection det1, det2, det3;

    det1.bbox = {92.0f, 92.0f, 20.0f, 20.0f};
    det1.confidence = 0.92f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;
    detections.push_back(det1);

    det2.bbox = {195.0f, 195.0f, 20.0f, 20.0f};
    det2.confidence = 0.88f;
    det2.classId = 1;
    det2.hitbox = HitboxType::Chest;
    detections.push_back(det2);

    det3.bbox = {300.0f, 300.0f, 20.0f, 20.0f};
    det3.confidence = 0.80f;
    det3.classId = 2;
    det3.hitbox = HitboxType::Body;
    detections.push_back(det3);

    DataAssociation::AssociationResult result = DataAssociation::matchDetections(db, detections, 0.3f);

    SECTION("Matched detections") {
        REQUIRE(result.matches.size() == 2);

        // Detection 0 should match target index 0 (id1)
        REQUIRE(result.matches[0].detectionIndex == 0);
        REQUIRE(result.matches[0].targetIndex == 0);

        // Detection 1 should match target index 1 (id2)
        REQUIRE(result.matches[1].detectionIndex == 1);
        REQUIRE(result.matches[1].targetIndex == 1);
    }

    SECTION("Unmatched detections (new targets)") {
        REQUIRE(result.unmatchedDetections.size() == 1);
        REQUIRE(result.unmatchedDetections[0] == 2);  // Detection index 2 (new target)
    }

    SECTION("Unmatched targets (lost tracks)") {
        REQUIRE(result.unmatchedTargets.empty());  // All targets were matched
    }
}

TEST_CASE("Data association with lost targets", "[tracking][data_association]") {
    TargetDatabase db;

    TargetID id1{1};
    TargetID id2{2};
    db.addTarget(id1, {100.0f, 100.0f}, {}, {90.0f, 90.0f, 20.0f, 20.0f}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {190.0f, 190.0f, 20.0f, 20.0f}, 0.85f, HitboxType::Chest, 0);

    // Only 1 detection (id2 disappears)
    std::vector<Detection> detections;
    Detection det1;
    det1.bbox = {92.0f, 92.0f, 20.0f, 20.0f};
    det1.confidence = 0.92f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;
    detections.push_back(det1);

    DataAssociation::AssociationResult result = DataAssociation::matchDetections(db, detections, 0.3f);

    REQUIRE(result.matches.size() == 1);
    REQUIRE(result.unmatchedDetections.empty());
    REQUIRE(result.unmatchedTargets.size() == 1);
    REQUIRE(result.unmatchedTargets[0] == 1);  // Target index 1 (id2) lost
}
