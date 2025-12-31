#include <catch2/catch_test_macros.hpp>
#include "config/SharedConfig.h"

using namespace macroman;

TEST_CASE("SharedConfig - Initialization", "[config][ipc]") {
    SharedConfig config;

    SECTION("Default values") {
        // Config defaults
        REQUIRE(config.aimSmoothness.load() == 0.5f);
        REQUIRE(config.fov.load() == 80.0f);
        REQUIRE(config.activeProfileId.load() == 0);
        REQUIRE(config.enablePrediction.load() == true);
        REQUIRE(config.enableAiming.load() == true);
        REQUIRE(config.enableTracking.load() == true);
        REQUIRE(config.enableTremor.load() == true);

        // Telemetry defaults
        REQUIRE(config.captureFPS.load() == 0.0f);
        REQUIRE(config.activeTargets.load() == 0);
        REQUIRE(config.vramUsageMB.load() == 0);

        // Safety metrics defaults
        REQUIRE(config.texturePoolStarved.load() == 0);
        REQUIRE(config.stalePredictionEvents.load() == 0);
        REQUIRE(config.deadmanSwitchTriggered.load() == 0);
    }

    SECTION("Reset method") {
        // Modify values
        config.aimSmoothness.store(0.8f);
        config.fov.store(120.0f);
        config.activeTargets.store(10);

        // Reset
        config.reset();

        // Verify defaults restored
        REQUIRE(config.aimSmoothness.load() == 0.5f);
        REQUIRE(config.fov.load() == 80.0f);
        REQUIRE(config.activeTargets.load() == 0);
    }
}

TEST_CASE("SharedConfig - Atomic operations", "[config][ipc]") {
    SharedConfig config;

    SECTION("Config field updates") {
        config.aimSmoothness.store(0.75f, std::memory_order_relaxed);
        REQUIRE(config.aimSmoothness.load(std::memory_order_relaxed) == 0.75f);

        config.fov.store(90.0f, std::memory_order_relaxed);
        REQUIRE(config.fov.load(std::memory_order_relaxed) == 90.0f);

        config.enablePrediction.store(false, std::memory_order_relaxed);
        REQUIRE(config.enablePrediction.load(std::memory_order_relaxed) == false);
    }

    SECTION("Telemetry updates") {
        config.captureFPS.store(144.0f, std::memory_order_relaxed);
        REQUIRE(config.captureFPS.load(std::memory_order_relaxed) == 144.0f);

        config.detectionLatency.store(8.5f, std::memory_order_relaxed);
        REQUIRE(config.detectionLatency.load(std::memory_order_relaxed) == 8.5f);

        config.activeTargets.store(3, std::memory_order_relaxed);
        REQUIRE(config.activeTargets.load(std::memory_order_relaxed) == 3);

        config.vramUsageMB.store(300, std::memory_order_relaxed);
        REQUIRE(config.vramUsageMB.load(std::memory_order_relaxed) == 300);
    }

    SECTION("Safety metric increments") {
        config.texturePoolStarved.fetch_add(1, std::memory_order_relaxed);
        REQUIRE(config.texturePoolStarved.load(std::memory_order_relaxed) == 1);

        config.stalePredictionEvents.fetch_add(5, std::memory_order_relaxed);
        REQUIRE(config.stalePredictionEvents.load(std::memory_order_relaxed) == 5);

        config.deadmanSwitchTriggered.fetch_add(1, std::memory_order_relaxed);
        REQUIRE(config.deadmanSwitchTriggered.load(std::memory_order_relaxed) == 1);
    }
}

TEST_CASE("ConfigSnapshot - Snapshot creation", "[config][ipc]") {
    SharedConfig config;

    // Set some values
    config.aimSmoothness.store(0.6f, std::memory_order_relaxed);
    config.fov.store(85.0f, std::memory_order_relaxed);
    config.captureFPS.store(120.0f, std::memory_order_relaxed);
    config.activeTargets.store(2, std::memory_order_relaxed);

    SECTION("Snapshot captures current state") {
        auto snap = ConfigSnapshot::snapshot(config);

        REQUIRE(snap.aimSmoothness == 0.6f);
        REQUIRE(snap.fov == 85.0f);
        REQUIRE(snap.captureFPS == 120.0f);
        REQUIRE(snap.activeTargets == 2);
    }

    SECTION("Snapshot is independent copy") {
        auto snap = ConfigSnapshot::snapshot(config);

        // Modify original
        config.aimSmoothness.store(0.9f, std::memory_order_relaxed);

        // Snapshot unchanged
        REQUIRE(snap.aimSmoothness == 0.6f);
        REQUIRE(config.aimSmoothness.load() == 0.9f);
    }
}

TEST_CASE("SharedConfig - Lock-free verification", "[config][ipc]") {
    // Static assertions already verify lock-free at compile-time
    // This test just documents the requirement

    SECTION("All atomics are lock-free on x64 Windows") {
        // These should always pass on x64 Windows (verified by static_assert)
        REQUIRE(std::atomic<float>::is_always_lock_free == true);
        REQUIRE(std::atomic<uint32_t>::is_always_lock_free == true);
        REQUIRE(std::atomic<bool>::is_always_lock_free == true);
        REQUIRE(std::atomic<int>::is_always_lock_free == true);
        REQUIRE(std::atomic<size_t>::is_always_lock_free == true);
        REQUIRE(std::atomic<uint64_t>::is_always_lock_free == true);
    }
}

TEST_CASE("SharedConfig - Alignment verification", "[config][ipc]") {
    SharedConfig config;

    SECTION("Cache-line alignment (64 bytes)") {
        REQUIRE(alignof(SharedConfig) == 64);
    }

    SECTION("Size is reasonable (< 2KB)") {
        // SharedConfig should fit in a few cache lines
        REQUIRE(sizeof(SharedConfig) < 2048);
    }
}
