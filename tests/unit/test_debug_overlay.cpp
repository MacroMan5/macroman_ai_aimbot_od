#include <catch2/catch_test_macros.hpp>
#include "../../src/ui/overlay/DebugOverlay.h"

using namespace macroman;

TEST_CASE("DebugOverlay - Initialization", "[ui][overlay]") {
    DebugOverlay overlay;

    SECTION("Initial state") {
        REQUIRE_FALSE(overlay.isInitialized());
    }

    SECTION("Default configuration") {
        auto config = overlay.getConfig();
        REQUIRE(config.enabled);
        REQUIRE(config.showMetrics);
        REQUIRE(config.showBBoxes);
        REQUIRE(config.showComponentToggles);
        REQUIRE_FALSE(config.showSafetyMetrics);  // Hidden by default
        REQUIRE(config.overlayAlpha == 0.9f);
    }
}

TEST_CASE("DebugOverlay - Configuration update", "[ui][overlay]") {
    DebugOverlay overlay;

    SECTION("Update config values") {
        DebugOverlay::Config newConfig;
        newConfig.enabled = false;
        newConfig.showMetrics = false;
        newConfig.overlayAlpha = 0.5f;
        newConfig.position = ImVec2(100.0f, 200.0f);

        overlay.updateConfig(newConfig);
        auto config = overlay.getConfig();

        REQUIRE_FALSE(config.enabled);
        REQUIRE_FALSE(config.showMetrics);
        REQUIRE(config.overlayAlpha == 0.5f);
        REQUIRE(config.position.x == 100.0f);
        REQUIRE(config.position.y == 200.0f);
    }
}

TEST_CASE("DebugOverlay - TargetSnapshot", "[ui][overlay]") {
    TargetSnapshot snapshot;

    SECTION("Initial state") {
        REQUIRE(snapshot.count == 0);
        REQUIRE(snapshot.selectedTargetIndex == SIZE_MAX);
    }

    SECTION("Clear operation") {
        snapshot.count = 5;
        snapshot.selectedTargetIndex = 2;

        snapshot.clear();

        REQUIRE(snapshot.count == 0);
        REQUIRE(snapshot.selectedTargetIndex == SIZE_MAX);
    }

    SECTION("Capacity check") {
        REQUIRE(TargetSnapshot::MAX_TARGETS == 64);
    }
}

TEST_CASE("DebugOverlay - TelemetrySnapshot", "[ui][overlay]") {
    TelemetrySnapshot telemetry;

    SECTION("Default initialization") {
        REQUIRE(telemetry.captureFPS == 0.0f);
        REQUIRE(telemetry.detectionLatency == 0.0f);
        REQUIRE(telemetry.activeTargets == 0);
        REQUIRE(telemetry.texturePoolStarved == 0);
        REQUIRE(telemetry.stalePredictionEvents == 0);
        REQUIRE(telemetry.deadmanSwitchTriggered == 0);
    }

    SECTION("Safety metrics tracking") {
        telemetry.texturePoolStarved = 5;
        telemetry.stalePredictionEvents = 100;
        telemetry.deadmanSwitchTriggered = 3;

        REQUIRE(telemetry.texturePoolStarved == 5);
        REQUIRE(telemetry.stalePredictionEvents == 100);
        REQUIRE(telemetry.deadmanSwitchTriggered == 3);
    }
}

TEST_CASE("DebugOverlay - Shutdown safety", "[ui][overlay]") {
    DebugOverlay overlay;

    SECTION("Shutdown uninitialized overlay") {
        // Should not crash
        overlay.shutdown();
        REQUIRE_FALSE(overlay.isInitialized());
    }

    SECTION("Multiple shutdowns") {
        overlay.shutdown();
        overlay.shutdown();  // Should not crash
        REQUIRE_FALSE(overlay.isInitialized());
    }
}
