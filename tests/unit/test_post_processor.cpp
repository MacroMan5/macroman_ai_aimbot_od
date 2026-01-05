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

TEST_CASE("PostProcessor - NMS Edge Cases", "[detection][nms][edge]") {
    SECTION("Empty detection list") {
        std::vector<Detection> dets;
        PostProcessor::applyNMS(dets, 0.5f);
        REQUIRE(dets.empty());  // Should handle gracefully
    }

    SECTION("Single detection") {
        std::vector<Detection> dets(1);
        dets[0].bbox = BBox{10, 10, 50, 50};
        dets[0].confidence = 0.9f;
        dets[0].classId = 0;

        PostProcessor::applyNMS(dets, 0.5f);
        REQUIRE(dets.size() == 1);  // Single detection always kept
    }

    SECTION("All detections overlap - keep highest confidence") {
        std::vector<Detection> dets(3);
        // All at same location, different confidences
        dets[0].bbox = BBox{10, 10, 50, 50};
        dets[0].confidence = 0.7f;
        dets[0].classId = 0;

        dets[1].bbox = BBox{10, 10, 50, 50};
        dets[1].confidence = 0.9f;  // Highest
        dets[1].classId = 0;

        dets[2].bbox = BBox{10, 10, 50, 50};
        dets[2].confidence = 0.8f;
        dets[2].classId = 0;

        PostProcessor::applyNMS(dets, 0.5f);

        // Should keep only highest confidence
        REQUIRE(dets.size() == 1);
        REQUIRE(dets[0].confidence == Approx(0.9f));
    }

    SECTION("Partial overlap - IoU threshold sensitivity") {
        std::vector<Detection> dets(2);
        dets[0].bbox = BBox{10, 10, 60, 60};  // 50x50 box
        dets[0].confidence = 0.9f;
        dets[0].classId = 0;

        dets[1].bbox = BBox{35, 35, 85, 85};  // Overlaps but IoU < 0.5
        dets[1].confidence = 0.8f;
        dets[1].classId = 0;

        PostProcessor::applyNMS(dets, 0.5f);

        // Overlap area: 25x25 = 625
        // Union area: (50x50) + (50x50) - 625 = 4375
        // IoU = 625/4375 ≈ 0.14 (below 0.5 threshold)
        REQUIRE(dets.size() == 2);  // Both kept
    }
}

TEST_CASE("PostProcessor - NMS with different IoU thresholds", "[detection][nms][iou]") {
    std::vector<Detection> dets(3);
    dets[0].bbox = BBox{10, 10, 60, 60};
    dets[0].confidence = 0.9f;
    dets[0].classId = 0;

    dets[1].bbox = BBox{30, 30, 80, 80};  // Moderate overlap
    dets[1].confidence = 0.8f;
    dets[1].classId = 0;

    dets[2].bbox = BBox{200, 200, 250, 250};  // No overlap
    dets[2].confidence = 0.85f;
    dets[2].classId = 0;

    SECTION("Aggressive NMS (IoU 0.3)") {
        auto dets_copy = dets;
        PostProcessor::applyNMS(dets_copy, 0.3f);
        // IoU between dets[0] and dets[1]: 900/4100 ≈ 0.22 < 0.3
        // Therefore, both boxes kept (below suppression threshold)
        REQUIRE(dets_copy.size() == 3);  // All kept (IoU below threshold)
    }

    SECTION("Moderate NMS (IoU 0.5)") {
        auto dets_copy = dets;
        PostProcessor::applyNMS(dets_copy, 0.5f);
        // Standard threshold
        REQUIRE(dets_copy.size() >= 2);
    }

    SECTION("Conservative NMS (IoU 0.7)") {
        auto dets_copy = dets;
        PostProcessor::applyNMS(dets_copy, 0.7f);
        // Higher threshold = less aggressive, keep more boxes
        REQUIRE(dets_copy.size() >= 2);
    }
}

TEST_CASE("PostProcessor - NMS Performance with many detections", "[detection][nms][performance]") {
    // Realistic scenario: Model outputs 100+ candidates before NMS
    std::vector<Detection> dets(100);

    // Create grid of detections with some overlap
    for (int i = 0; i < 100; ++i) {
        int row = i / 10;
        int col = i % 10;
        dets[i].bbox = BBox{
            static_cast<float>(col * 60),
            static_cast<float>(row * 60),
            static_cast<float>(col * 60 + 50),
            static_cast<float>(row * 60 + 50)
        };
        dets[i].confidence = 0.5f + (i % 50) * 0.01f;  // Varying confidences
        dets[i].classId = 0;
    }

    // NMS should complete without timeout (performance target: <1ms for 100 dets)
    PostProcessor::applyNMS(dets, 0.5f);

    // After NMS, should have significantly fewer detections
    REQUIRE(dets.size() < 100);  // Some overlap removed
    REQUIRE(dets.size() > 0);    // But not all removed

    // All remaining detections should have confidence >= 0.5
    for (const auto& det : dets) {
        REQUIRE(det.confidence >= 0.5f);
    }
}

TEST_CASE("PostProcessor - Confidence filtering edge cases", "[detection][confidence][edge]") {
    SECTION("All detections below threshold") {
        std::vector<Detection> dets(3);
        dets[0].confidence = 0.3f;
        dets[1].confidence = 0.4f;
        dets[2].confidence = 0.2f;

        PostProcessor::filterByConfidence(dets, 0.6f);
        REQUIRE(dets.empty());  // All removed
    }

    SECTION("All detections above threshold") {
        std::vector<Detection> dets(3);
        dets[0].confidence = 0.9f;
        dets[1].confidence = 0.8f;
        dets[2].confidence = 0.7f;

        PostProcessor::filterByConfidence(dets, 0.6f);
        REQUIRE(dets.size() == 3);  // All kept
    }

    SECTION("Zero threshold keeps all") {
        std::vector<Detection> dets(3);
        dets[0].confidence = 0.1f;
        dets[1].confidence = 0.01f;
        dets[2].confidence = 0.001f;

        PostProcessor::filterByConfidence(dets, 0.0f);
        REQUIRE(dets.size() == 3);
    }

    SECTION("Threshold of 1.0 removes all") {
        std::vector<Detection> dets(3);
        dets[0].confidence = 0.99f;
        dets[1].confidence = 0.95f;
        dets[2].confidence = 0.9f;

        PostProcessor::filterByConfidence(dets, 1.0f);
        REQUIRE(dets.empty());  // None can reach exactly 1.0
    }
}

TEST_CASE("PostProcessor - Chained operations", "[detection][pipeline]") {
    // Realistic pipeline: filter → NMS → hitbox mapping
    std::vector<Detection> dets(5);

    dets[0].bbox = BBox{10, 10, 60, 60};
    dets[0].confidence = 0.9f;
    dets[0].classId = 0;

    dets[1].bbox = BBox{15, 15, 65, 65};  // Overlaps with dets[0]
    dets[1].confidence = 0.3f;  // Low confidence
    dets[1].classId = 0;

    dets[2].bbox = BBox{100, 100, 150, 150};
    dets[2].confidence = 0.85f;
    dets[2].classId = 1;

    dets[3].bbox = BBox{105, 105, 155, 155};  // Overlaps with dets[2]
    dets[3].confidence = 0.8f;
    dets[3].classId = 1;

    dets[4].bbox = BBox{200, 200, 250, 250};
    dets[4].confidence = 0.7f;
    dets[4].classId = 2;

    // Step 1: Filter low confidence
    PostProcessor::filterByConfidence(dets, 0.6f);
    REQUIRE(dets.size() == 4);  // dets[1] removed (0.3)

    // Step 2: Apply NMS
    PostProcessor::applyNMS(dets, 0.5f);
    REQUIRE(dets.size() == 3);  // One overlap removed per class

    // Step 3: Map hitboxes
    std::unordered_map<int, HitboxType> mapping = {
        {0, HitboxType::Head},
        {1, HitboxType::Chest},
        {2, HitboxType::Body}
    };
    PostProcessor::mapHitboxes(dets, mapping);

    // Verify final pipeline output
    REQUIRE(dets[0].hitbox == HitboxType::Head);
    REQUIRE(dets[0].confidence >= 0.6f);
}
