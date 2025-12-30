#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/TargetSelector.h"
#include "core/entities/TargetDatabase.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("Target selection filters by FOV", "[tracking][selector]") {
    TargetDatabase db;
    db.count = 3;

    // Crosshair at center (960, 540) for 1920x1080 screen
    Vec2 crosshair{960.0f, 540.0f};
    
    // Target 1: Inside FOV (50 pixels away)
    db.positions[0] = {1000.0f, 540.0f};
    db.confidences[0] = 0.9f;
    db.hitboxTypes[0] = HitboxType::Body;
    
    // Target 2: Outside FOV (200 pixels away)
    db.positions[1] = {1160.0f, 540.0f};
    db.confidences[1] = 0.95f;
    db.hitboxTypes[1] = HitboxType::Head;
    
    // Target 3: Inside FOV (30 pixels away)
    db.positions[2] = {990.0f, 540.0f};
    db.confidences[2] = 0.85f;
    db.hitboxTypes[2] = HitboxType::Chest;

    TargetSelector selector;
    auto selected = selector.selectBest(db, crosshair, 100.0f);  // FOV radius = 100px

    REQUIRE(selected.has_value());
    REQUIRE(selected->targetIndex == 2);  // Target 3 is closest inside FOV
}

TEST_CASE("Target selection prioritizes hitbox type", "[tracking][selector]") {
    TargetDatabase db;
    db.count = 3;

    Vec2 crosshair{960.0f, 540.0f};
    
    // All targets at same distance (50 pixels)
    db.positions[0] = {1010.0f, 540.0f};
    db.confidences[0] = 0.9f;
    db.hitboxTypes[0] = HitboxType::Body;
    
    db.positions[1] = {960.0f, 590.0f};
    db.confidences[1] = 0.9f;
    db.hitboxTypes[1] = HitboxType::Chest;
    
    db.positions[2] = {910.0f, 540.0f};
    db.confidences[2] = 0.9f;
    db.hitboxTypes[2] = HitboxType::Head;

    TargetSelector selector;
    auto selected = selector.selectBest(db, crosshair, 100.0f);

    REQUIRE(selected.has_value());
    REQUIRE(selected->targetIndex == 2);  // Head hitbox wins
    REQUIRE(selected->hitbox == HitboxType::Head);
}

TEST_CASE("Target selection returns nullopt when no targets in FOV", "[tracking][selector]") {
    TargetDatabase db;
    db.count = 2;

    Vec2 crosshair{960.0f, 540.0f};
    
    // Both targets outside FOV
    db.positions[0] = {1200.0f, 540.0f};
    db.confidences[0] = 0.9f;
    db.hitboxTypes[0] = HitboxType::Head;
    
    db.positions[1] = {700.0f, 540.0f};
    db.confidences[1] = 0.9f;
    db.hitboxTypes[1] = HitboxType::Head;

    TargetSelector selector;
    auto selected = selector.selectBest(db, crosshair, 100.0f);

    REQUIRE_FALSE(selected.has_value());
}

TEST_CASE("Target selection handles empty database", "[tracking][selector]") {
    TargetDatabase db;
    db.count = 0;

    Vec2 crosshair{960.0f, 540.0f};
    
    TargetSelector selector;
    auto selected = selector.selectBest(db, crosshair, 100.0f);

    REQUIRE_FALSE(selected.has_value());
}
