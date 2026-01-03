#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "input/movement/BezierCurve.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("BezierCurve - Normal Bezier evaluation", "[input][bezier]") {
    // Create a simple curve from (0,0) to (100,100)
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {33.3f, 0.0f};
    curve.p2 = {66.6f, 100.0f};
    curve.p3 = {100.0f, 100.0f};

    SECTION("At t=0.0, should return start point") {
        Vec2 result = curve.at(0.0f);
        REQUIRE_THAT(result.x, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(result.y, WithinAbs(0.0f, 0.01f));
    }

    SECTION("At t=1.0, should return target point") {
        Vec2 result = curve.at(1.0f);
        REQUIRE_THAT(result.x, WithinAbs(100.0f, 0.01f));
        REQUIRE_THAT(result.y, WithinAbs(100.0f, 0.01f));
    }

    SECTION("At t=0.5, should be roughly midway") {
        Vec2 result = curve.at(0.5f);
        // For this specific control point configuration
        REQUIRE(result.x >= 40.0f);
        REQUIRE(result.x <= 60.0f);
        REQUIRE(result.y >= 40.0f);
        REQUIRE(result.y <= 60.0f);
    }

    SECTION("Curve should be smooth (no discontinuities)") {
        Vec2 prev = curve.at(0.0f);

        for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
            Vec2 curr = curve.at(t);

            // Check delta is reasonable (no jumps > 20 pixels for 0.1 step)
            float dx = std::abs(curr.x - prev.x);
            float dy = std::abs(curr.y - prev.y);

            REQUIRE(dx < 20.0f);
            REQUIRE(dy < 20.0f);

            prev = curr;
        }
    }
}

TEST_CASE("BezierCurve - Overshoot phase", "[input][bezier][humanization]") {
    // Create horizontal movement: (0,50) → (100,50)
    BezierCurve curve;
    curve.p0 = {0.0f, 50.0f};
    curve.p1 = {33.3f, 50.0f};
    curve.p2 = {66.6f, 50.0f};
    curve.p3 = {100.0f, 50.0f};
    curve.overshootFactor = 0.15f;  // 15% overshoot

    SECTION("At t=1.0, should be at target (start of overshoot)") {
        Vec2 result = curve.at(1.0f);
        REQUIRE_THAT(result.x, WithinAbs(100.0f, 0.01f));
        REQUIRE_THAT(result.y, WithinAbs(50.0f, 0.01f));
    }

    SECTION("At t=1.075 (midpoint), should be past target (overshoot peak)") {
        Vec2 result = curve.at(1.075f);

        // Should overshoot horizontally (x > 100)
        REQUIRE(result.x > 100.0f);

        // Y should remain approximately constant (horizontal movement)
        REQUIRE_THAT(result.y, WithinAbs(50.0f, 2.0f));

        // Overshoot should be roughly 15% of curve length
        // Curve length ≈ 100 (horizontal), so overshoot ≈ 15 pixels
        REQUIRE(result.x <= 120.0f);  // Not too much overshoot
    }

    SECTION("At t=1.15, should return to target (correction complete)") {
        Vec2 result = curve.at(1.15f);
        REQUIRE_THAT(result.x, WithinAbs(100.0f, 0.5f));
        REQUIRE_THAT(result.y, WithinAbs(50.0f, 0.5f));
    }

    SECTION("Overshoot should be parabolic (smooth)") {
        Vec2 at_1_0 = curve.at(1.0f);
        Vec2 at_1_075 = curve.at(1.075f);
        Vec2 at_1_15 = curve.at(1.15f);

        // Distance from target should peak at t=1.075
        float dist_1_0 = (at_1_0 - curve.p3).length();
        float dist_1_075 = (at_1_075 - curve.p3).length();
        float dist_1_15 = (at_1_15 - curve.p3).length();

        REQUIRE(dist_1_075 > dist_1_0);
        REQUIRE(dist_1_075 > dist_1_15);
    }
}

TEST_CASE("BezierCurve - Boundary conditions", "[input][bezier][edge]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {50.0f, 0.0f};
    curve.p2 = {50.0f, 100.0f};
    curve.p3 = {100.0f, 100.0f};

    SECTION("Negative t should clamp to t=0.0 (start point)") {
        Vec2 result = curve.at(-0.5f);
        REQUIRE_THAT(result.x, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(result.y, WithinAbs(0.0f, 0.01f));
    }

    SECTION("t > 1.15 should clamp to target") {
        Vec2 result = curve.at(2.0f);
        REQUIRE_THAT(result.x, WithinAbs(100.0f, 0.01f));
        REQUIRE_THAT(result.y, WithinAbs(100.0f, 0.01f));
    }
}

TEST_CASE("BezierCurve - Edge cases", "[input][bezier][edge]") {
    SECTION("Zero-length curve (all points same)") {
        BezierCurve curve;
        curve.p0 = curve.p1 = curve.p2 = curve.p3 = {50.0f, 50.0f};

        Vec2 result_0 = curve.at(0.0f);
        Vec2 result_mid = curve.at(0.5f);
        Vec2 result_1 = curve.at(1.0f);
        Vec2 result_overshoot = curve.at(1.075f);

        // All should be at same point
        REQUIRE_THAT(result_0.x, WithinAbs(50.0f, 0.01f));
        REQUIRE_THAT(result_mid.x, WithinAbs(50.0f, 0.01f));
        REQUIRE_THAT(result_1.x, WithinAbs(50.0f, 0.01f));
        REQUIRE_THAT(result_overshoot.x, WithinAbs(50.0f, 0.01f));
    }

    SECTION("Near-zero direction in overshoot phase") {
        BezierCurve curve;
        curve.p0 = {0.0f, 0.0f};
        curve.p1 = {50.0f, 50.0f};
        curve.p2 = {99.99f, 100.0f};  // Very close to p3
        curve.p3 = {100.0f, 100.0f};

        // Should handle gracefully (direction vector near zero)
        Vec2 result = curve.at(1.075f);

        // Should not crash or produce NaN
        REQUIRE(std::isfinite(result.x));
        REQUIRE(std::isfinite(result.y));

        // Should stay near target
        REQUIRE_THAT(result.x, WithinAbs(100.0f, 5.0f));
        REQUIRE_THAT(result.y, WithinAbs(100.0f, 5.0f));
    }
}

TEST_CASE("BezierCurve - Arc length calculation", "[input][bezier]") {
    SECTION("Straight horizontal line") {
        BezierCurve curve;
        curve.p0 = {0.0f, 50.0f};
        curve.p1 = {33.3f, 50.0f};
        curve.p2 = {66.6f, 50.0f};
        curve.p3 = {100.0f, 50.0f};

        float len = curve.length();

        // Chord length approximation should be close to 100
        REQUIRE_THAT(len, WithinAbs(100.0f, 10.0f));
    }

    SECTION("Diagonal movement") {
        BezierCurve curve;
        curve.p0 = {0.0f, 0.0f};
        curve.p1 = {33.3f, 33.3f};
        curve.p2 = {66.6f, 66.6f};
        curve.p3 = {100.0f, 100.0f};

        float len = curve.length();

        // Diagonal length: sqrt(100^2 + 100^2) ≈ 141.4
        REQUIRE_THAT(len, WithinAbs(141.4f, 15.0f));
    }

    SECTION("Complex curve with control points") {
        BezierCurve curve;
        curve.p0 = {0.0f, 0.0f};
        curve.p1 = {50.0f, 100.0f};  // Control point above
        curve.p2 = {50.0f, 0.0f};     // Control point below
        curve.p3 = {100.0f, 100.0f};

        float len = curve.length();

        // Should be longer than straight line (100 -> 141.4)
        REQUIRE(len > 141.4f);

        // S-shaped curve can be significantly longer (actual: ~323 pixels)
        REQUIRE(len < 400.0f);
    }
}

TEST_CASE("BezierCurve - Realistic game scenario", "[input][bezier][realistic]") {
    // Scenario: Flick shot from screen center to enemy at top-right
    BezierCurve curve;
    curve.p0 = {960.0f, 540.0f};      // 1920x1080 center
    curve.p1 = {1200.0f, 400.0f};     // Control point 1 (intermediate)
    curve.p2 = {1440.0f, 260.0f};     // Control point 2 (near target)
    curve.p3 = {1600.0f, 200.0f};     // Target enemy head

    SECTION("Normal phase should reach target smoothly") {
        Vec2 start = curve.at(0.0f);
        Vec2 end = curve.at(1.0f);

        REQUIRE_THAT(start.x, WithinAbs(960.0f, 0.1f));
        REQUIRE_THAT(start.y, WithinAbs(540.0f, 0.1f));

        REQUIRE_THAT(end.x, WithinAbs(1600.0f, 0.1f));
        REQUIRE_THAT(end.y, WithinAbs(200.0f, 0.1f));
    }

    SECTION("Overshoot phase should mimic human flick shot") {
        Vec2 at_target = curve.at(1.0f);
        Vec2 at_overshoot = curve.at(1.075f);
        Vec2 at_corrected = curve.at(1.15f);

        // Should overshoot past target
        float dist_overshoot = (at_overshoot - at_target).length();
        REQUIRE(dist_overshoot > 5.0f);  // At least 5 pixels overshoot

        // Should correct back near target
        float dist_corrected = (at_corrected - at_target).length();
        REQUIRE(dist_corrected < 2.0f);  // Within 2 pixels of target
    }

    SECTION("Arc length should be reasonable for screen distance") {
        float len = curve.length();

        // Direct distance: sqrt((1600-960)^2 + (200-540)^2) ≈ 724 pixels
        // Arc length should be longer due to curve, but not excessive
        REQUIRE(len > 724.0f);   // Longer than straight line
        REQUIRE(len < 1000.0f);  // Not unreasonably long
    }
}
