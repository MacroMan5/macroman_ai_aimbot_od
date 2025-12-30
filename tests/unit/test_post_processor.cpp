#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "detection/postprocess/PostProcessor.h"

using namespace macroman;
using Catch::Approx;

TEST_CASE("PostProcessor - NMS", "[detection]") {
    SECTION("Remove overlapping detections") {
        std::vector<Detection> dets(3);
        dets[0].bbox = BBox{10, 10, 50, 50};
        dets[0].confidence = 0.9f;
        dets[0].classId = 0;
        dets[0].hitbox = HitboxType::Unknown;

        dets[1].bbox = BBox{15, 15, 55, 55};  // Overlaps with first (IoU > 0.5)
        dets[1].confidence = 0.8f;
        dets[1].classId = 0;
        dets[1].hitbox = HitboxType::Unknown;

        dets[2].bbox = BBox{200, 200, 250, 250};  // No overlap
        dets[2].confidence = 0.85f;
        dets[2].classId = 0;
        dets[2].hitbox = HitboxType::Unknown;

        PostProcessor::applyNMS(dets, 0.5f);

        // Should keep 2 detections (first and third)
        REQUIRE(dets.size() == 2);
        REQUIRE(dets[0].confidence == Approx(0.9f));   // Highest confidence kept
        REQUIRE(dets[1].confidence == Approx(0.85f));  // Non-overlapping kept
    }

    SECTION("Keep all detections when no overlap") {
        std::vector<Detection> dets(3);
        dets[0].bbox = BBox{10, 10, 50, 50};
        dets[0].confidence = 0.9f;
        dets[0].classId = 0;

        dets[1].bbox = BBox{100, 100, 150, 150};
        dets[1].confidence = 0.8f;
        dets[1].classId = 0;

        dets[2].bbox = BBox{200, 200, 250, 250};
        dets[2].confidence = 0.85f;
        dets[2].classId = 0;

        PostProcessor::applyNMS(dets, 0.5f);

        REQUIRE(dets.size() == 3);  // All kept
    }
}

TEST_CASE("PostProcessor - Confidence Filtering", "[detection]") {
    std::vector<Detection> dets(3);
    dets[0].bbox = BBox{10, 10, 50, 50};
    dets[0].confidence = 0.9f;
    dets[0].classId = 0;

    dets[1].bbox = BBox{100, 100, 150, 150};  // Below threshold
    dets[1].confidence = 0.4f;
    dets[1].classId = 0;

    dets[2].bbox = BBox{200, 200, 250, 250};
    dets[2].confidence = 0.7f;
    dets[2].classId = 0;

    PostProcessor::filterByConfidence(dets, 0.6f);

    REQUIRE(dets.size() == 2);
    REQUIRE(dets[0].confidence == Approx(0.9f));
    REQUIRE(dets[1].confidence == Approx(0.7f));
}

TEST_CASE("PostProcessor - Hitbox Mapping", "[detection]") {
    std::vector<Detection> dets(4);
    dets[0].confidence = 0.9f;
    dets[0].classId = 0;  // classId 0
    dets[0].hitbox = HitboxType::Unknown;

    dets[1].confidence = 0.8f;
    dets[1].classId = 1;  // classId 1
    dets[1].hitbox = HitboxType::Unknown;

    dets[2].confidence = 0.7f;
    dets[2].classId = 2;  // classId 2
    dets[2].hitbox = HitboxType::Unknown;

    dets[3].confidence = 0.6f;
    dets[3].classId = 99;  // classId 99 (unmapped)
    dets[3].hitbox = HitboxType::Unknown;

    std::unordered_map<int, HitboxType> mapping = {
        {0, HitboxType::Head},
        {1, HitboxType::Chest},
        {2, HitboxType::Body}
    };

    PostProcessor::mapHitboxes(dets, mapping);

    REQUIRE(dets[0].hitbox == HitboxType::Head);
    REQUIRE(dets[1].hitbox == HitboxType::Chest);
    REQUIRE(dets[2].hitbox == HitboxType::Body);
    REQUIRE(dets[3].hitbox == HitboxType::Unknown);  // Unmapped classId
}
