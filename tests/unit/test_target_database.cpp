#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/TargetDatabase.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("TargetDatabase construction", "[entities][target_database]") {
    TargetDatabase db;

    REQUIRE(db.count == 0);
    REQUIRE(db.getCount() == 0);
    REQUIRE(db.isEmpty());
    REQUIRE_FALSE(db.isFull());
}

TEST_CASE("TargetDatabase add target", "[entities][target_database]") {
    TargetDatabase db;

    TargetID id{1};
    Vec2 pos{100.0f, 200.0f};
    Vec2 vel{5.0f, -3.0f};
    BBox bbox{95.0f, 195.0f, 10.0f, 10.0f};
    float confidence = 0.9f;
    HitboxType hitbox = HitboxType::Head;
    int64_t timestamp = 1000000000;

    size_t index = db.addTarget(id, pos, vel, bbox, confidence, hitbox, timestamp);

    REQUIRE(index == 0);
    REQUIRE(db.count == 1);
    REQUIRE(db.ids[0] == id);
    REQUIRE_THAT(db.positions[0].x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(db.velocities[0].y, WithinAbs(-3.0f, 0.001f));
    REQUIRE(db.hitboxTypes[0] == HitboxType::Head);
}

TEST_CASE("TargetDatabase prediction update", "[entities][target_database]") {
    TargetDatabase db;
    db.addTarget(TargetID{1}, {100.0f, 100.0f}, {100.0f, 0.0f}, {}, 0.9f, HitboxType::Body, 0);

    // Update prediction by 0.1s: 100 + 100 * 0.1 = 110
    db.updatePredictions(0.1f);

    REQUIRE_THAT(db.positions[0].x, WithinAbs(110.0f, 0.001f));
    REQUIRE_THAT(db.positions[0].y, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("TargetDatabase remove target", "[entities][target_database]") {
    TargetDatabase db;

    TargetID id1{1};
    TargetID id2{2};
    TargetID id3{3};

    db.addTarget(id1, {100.0f, 100.0f}, {}, {}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {}, 0.9f, HitboxType::Chest, 0);
    db.addTarget(id3, {300.0f, 300.0f}, {}, {}, 0.9f, HitboxType::Body, 0);

    REQUIRE(db.count == 3);

    // Remove middle target (id2 at index 1)
    bool removed = db.removeTarget(1);

    REQUIRE(removed);
    REQUIRE(db.count == 2);

    // id3 should now be at index 1 (swap-erase)
    REQUIRE(db.ids[1] == id3);
    REQUIRE_THAT(db.positions[1].x, WithinAbs(300.0f, 0.001f));
}
