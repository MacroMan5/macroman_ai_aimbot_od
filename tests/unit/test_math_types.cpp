#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/MathTypes.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 basic operations", "[math][vec2]") {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    SECTION("Addition") {
        Vec2 result = a + b;
        REQUIRE_THAT(result.x, WithinAbs(4.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(6.0f, 0.001f));
    }

    SECTION("Subtraction") {
        Vec2 result = a - b;
        REQUIRE_THAT(result.x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Scalar multiplication") {
        Vec2 result = a * 2.0f;
        REQUIRE_THAT(result.x, WithinAbs(6.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(8.0f, 0.001f));
    }

    SECTION("Scalar division") {
        Vec2 result = a / 2.0f;
        REQUIRE_THAT(result.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Length") {
        float len = a.length();
        REQUIRE_THAT(len, WithinAbs(5.0f, 0.001f));  // sqrt(3^2 + 4^2) = 5
    }

    SECTION("Distance") {
        float dist = Vec2::distance(a, b);
        REQUIRE_THAT(dist, WithinAbs(2.828f, 0.01f));  // sqrt((3-1)^2 + (4-2)^2)
    }

    SECTION("Normalized") {
        Vec2 norm = a.normalized();
        REQUIRE_THAT(norm.x, WithinAbs(0.6f, 0.001f));
        REQUIRE_THAT(norm.y, WithinAbs(0.8f, 0.001f));
        REQUIRE_THAT(norm.length(), WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("TargetID comparison", "[math][targetid]") {
    TargetID id1{42};
    TargetID id2{42};
    TargetID id3{99};

    REQUIRE(id1 == id2);
    REQUIRE(id1 != id3);
    REQUIRE(id1.value == 42);
}
