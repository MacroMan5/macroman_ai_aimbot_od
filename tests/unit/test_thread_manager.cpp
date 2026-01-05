/**
 * @file test_thread_manager.cpp
 * @brief Unit tests for ThreadManager thread affinity API (Phase 8 - P8-03)
 *
 * Tests:
 * - CPU core count detection
 * - Thread affinity setting (valid/invalid parameters)
 * - 6+ core requirement check
 * - Boundary conditions
 */

#include <catch2/catch_test_macros.hpp>
#include "core/threading/ThreadManager.h"
#include <thread>
#include <chrono>

using namespace macroman;

TEST_CASE("ThreadManager - CPU Core Count Detection", "[thread-manager][affinity]") {
    SECTION("getCPUCoreCount returns valid number") {
        unsigned int coreCount = ThreadManager::getCPUCoreCount();
        REQUIRE(coreCount >= 1);  // At least 1 core
        REQUIRE(coreCount <= 256);  // Reasonable upper bound

        // Should match std::thread::hardware_concurrency()
        unsigned int stdCoreCount = std::thread::hardware_concurrency();
        if (stdCoreCount > 0) {
            REQUIRE(coreCount == stdCoreCount);
        }
    }
}

TEST_CASE("ThreadManager - Thread Affinity API", "[thread-manager][affinity]") {
    ThreadManager manager;

    // Create a simple test thread
    bool threadRan = false;
    manager.createThread("TestThread", 0, [&](std::atomic<bool>& stop) {
        threadRan = true;
        while (!stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Give thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    SECTION("Valid thread index and core ID") {
        unsigned int coreCount = ThreadManager::getCPUCoreCount();

        if (coreCount >= 6) {
            // Should succeed on 6+ core systems
            bool result = manager.setCoreAffinity(0, 1);  // Bind to Core 1
            REQUIRE(result == true);
        } else {
            // Should skip on <6 core systems
            bool result = manager.setCoreAffinity(0, 1);
            REQUIRE(result == false);  // Skipped due to core count check
        }
    }

    SECTION("Invalid thread index") {
        bool result = manager.setCoreAffinity(999, 0);  // Index out of range
        REQUIRE(result == false);
    }

    SECTION("Invalid core ID (negative)") {
        unsigned int coreCount = ThreadManager::getCPUCoreCount();

        if (coreCount >= 6) {
            bool result = manager.setCoreAffinity(0, -1);  // Negative core ID
            REQUIRE(result == false);
        }
    }

    SECTION("Invalid core ID (out of range)") {
        unsigned int coreCount = ThreadManager::getCPUCoreCount();

        if (coreCount >= 6) {
            bool result = manager.setCoreAffinity(0, static_cast<int>(coreCount) + 10);  // Beyond max
            REQUIRE(result == false);
        }
    }

    SECTION("Core count boundary (5 cores = skip, 6 cores = allow)") {
        // This test verifies the 6+ core requirement logic
        // On a 6+ core system, affinity should work
        // On a <6 core system, affinity should be skipped
        unsigned int coreCount = ThreadManager::getCPUCoreCount();
        INFO("System has " << coreCount << " cores");

        bool result = manager.setCoreAffinity(0, 0);  // Try to bind to Core 0

        if (coreCount >= 6) {
            REQUIRE(result == true);  // Should succeed
        } else {
            REQUIRE(result == false);  // Should skip
        }
    }

    // Cleanup
    manager.stopAll();
    REQUIRE(threadRan == true);  // Verify thread actually ran
}

TEST_CASE("ThreadManager - Thread Affinity Edge Cases", "[thread-manager][affinity]") {
    ThreadManager manager;

    SECTION("Set affinity with no threads") {
        bool result = manager.setCoreAffinity(0, 0);
        REQUIRE(result == false);  // Should fail - no thread at index 0
    }

    SECTION("Multiple threads with different affinities") {
        unsigned int coreCount = ThreadManager::getCPUCoreCount();

        // Only test on 6+ core systems
        if (coreCount >= 6) {
            // Create 4 threads
            for (int i = 0; i < 4; ++i) {
                manager.createThread("Thread" + std::to_string(i), 0, [](std::atomic<bool>& stop) {
                    while (!stop.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                });
            }

            // Give threads time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Pin to cores 1-4 (avoid Core 0 - OS interrupts)
            REQUIRE(manager.setCoreAffinity(0, 1) == true);
            REQUIRE(manager.setCoreAffinity(1, 2) == true);
            REQUIRE(manager.setCoreAffinity(2, 3) == true);
            REQUIRE(manager.setCoreAffinity(3, 4) == true);
        }

        manager.stopAll();
    }
}
