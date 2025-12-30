#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/AimCommand.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("AimCommand default construction", "[core][aim]") {
    AimCommand cmd;

    REQUIRE_FALSE(cmd.hasTarget);
    REQUIRE(cmd.targetPosition.x == 0.0f);
    REQUIRE(cmd.targetPosition.y == 0.0f);
    REQUIRE(cmd.confidence == 0.0f);
    REQUIRE(cmd.hitbox == HitboxType::Body);
}

TEST_CASE("AimCommand with valid target", "[core][aim]") {
    AimCommand cmd;
    cmd.hasTarget = true;
    cmd.targetPosition = {1920.0f / 2.0f, 1080.0f / 2.0f};  // Screen center
    cmd.confidence = 0.95f;
    cmd.hitbox = HitboxType::Head;

    REQUIRE(cmd.hasTarget);
    REQUIRE_THAT(cmd.targetPosition.x, WithinAbs(960.0f, 0.1f));
    REQUIRE_THAT(cmd.targetPosition.y, WithinAbs(540.0f, 0.1f));
    REQUIRE(cmd.confidence > 0.9f);
    REQUIRE(cmd.hitbox == HitboxType::Head);
}

TEST_CASE("AimCommand copy semantics", "[core][aim]") {
    AimCommand cmd1;
    cmd1.hasTarget = true;
    cmd1.targetPosition = {100.0f, 200.0f};
    cmd1.confidence = 0.8f;
    cmd1.hitbox = HitboxType::Chest;

    AimCommand cmd2 = cmd1;  // Copy

    REQUIRE(cmd2.hasTarget == cmd1.hasTarget);
    REQUIRE(cmd2.targetPosition.x == cmd1.targetPosition.x);
    REQUIRE(cmd2.targetPosition.y == cmd1.targetPosition.y);
    REQUIRE(cmd2.confidence == cmd1.confidence);
    REQUIRE(cmd2.hitbox == cmd1.hitbox);
}
