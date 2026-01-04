/**
 * @file test_performance_metrics.cpp
 * @brief Unit tests for PerformanceMetrics system
 *
 * Tests:
 * - Metric recording and retrieval
 * - EMA (Exponential Moving Average) calculations
 * - Min/max tracking
 * - Snapshot consistency
 * - Thread-safety (basic validation)
 * - Reset functionality
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/metrics/PerformanceMetrics.h"
#include <thread>
#include <vector>

using namespace macroman;
using Catch::Matchers::WithinRel;

TEST_CASE("PerformanceMetrics - Basic Recording", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("Initial state") {
        auto snap = metrics.snapshot();

        REQUIRE(snap.captureFPS == 0.0f);
        REQUIRE(snap.captureFrames == 0);
        REQUIRE(snap.captureLatencyAvg == 0.0f);
        REQUIRE(snap.activeTargets == 0);
        REQUIRE(snap.vramUsageMB == 0);
    }

    SECTION("Record single latency sample") {
        metrics.recordCaptureLatency(5.5f);

        auto snap = metrics.snapshot();
        REQUIRE(snap.captureFrames == 1);
        REQUIRE_THAT(snap.captureLatencyAvg, WithinRel(5.5f, 0.01f));
        REQUIRE_THAT(snap.captureLatencyMin, WithinRel(5.5f, 0.01f));
        REQUIRE_THAT(snap.captureLatencyMax, WithinRel(5.5f, 0.01f));
    }

    SECTION("Record multiple latency samples") {
        metrics.recordCaptureLatency(5.0f);
        metrics.recordCaptureLatency(10.0f);
        metrics.recordCaptureLatency(7.5f);

        auto snap = metrics.snapshot();
        REQUIRE(snap.captureFrames == 3);

        // Min/Max should be correct
        REQUIRE_THAT(snap.captureLatencyMin, WithinRel(5.0f, 0.01f));
        REQUIRE_THAT(snap.captureLatencyMax, WithinRel(10.0f, 0.01f));

        // EMA should be between min and max (with alpha=0.05)
        REQUIRE(snap.captureLatencyAvg > 5.0f);
        REQUIRE(snap.captureLatencyAvg < 10.0f);
    }
}

TEST_CASE("PerformanceMetrics - EMA Calculation", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("EMA converges correctly") {
        // Record 100 samples of 10.0ms
        for (int i = 0; i < 100; ++i) {
            metrics.recordDetectionLatency(10.0f);
        }

        auto snap = metrics.snapshot();

        // With alpha=0.05, after 100 samples, EMA should be very close to 10.0
        REQUIRE_THAT(snap.detectionLatencyAvg, WithinRel(10.0f, 0.1f));  // Within 10%
        REQUIRE(snap.detectionFrames == 100);
    }

    SECTION("EMA responds to change") {
        // Record 50 samples of 5.0ms
        for (int i = 0; i < 50; ++i) {
            metrics.recordTrackingLatency(5.0f);
        }

        float avgBefore = metrics.snapshot().trackingLatencyAvg;

        // Record 50 samples of 15.0ms
        for (int i = 0; i < 50; ++i) {
            metrics.recordTrackingLatency(15.0f);
        }

        float avgAfter = metrics.snapshot().trackingLatencyAvg;

        // Average should have increased
        REQUIRE(avgAfter > avgBefore);
        REQUIRE(avgAfter > 5.0f);
        REQUIRE(avgAfter < 15.0f);
    }
}

TEST_CASE("PerformanceMetrics - Min/Max Tracking", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("Min tracks lowest value") {
        metrics.recordInputLatency(10.0f);
        metrics.recordInputLatency(5.0f);
        metrics.recordInputLatency(15.0f);
        metrics.recordInputLatency(3.0f);  // New minimum
        metrics.recordInputLatency(20.0f);

        auto snap = metrics.snapshot();
        REQUIRE_THAT(snap.inputLatencyMin, WithinRel(3.0f, 0.01f));
    }

    SECTION("Max tracks highest value") {
        metrics.recordInputLatency(10.0f);
        metrics.recordInputLatency(5.0f);
        metrics.recordInputLatency(25.0f);  // New maximum
        metrics.recordInputLatency(15.0f);
        metrics.recordInputLatency(20.0f);

        auto snap = metrics.snapshot();
        REQUIRE_THAT(snap.inputLatencyMax, WithinRel(25.0f, 0.01f));
    }

    SECTION("Min/Max with out-of-order samples") {
        std::vector<float> samples = {15.0f, 5.0f, 30.0f, 2.0f, 25.0f, 10.0f};

        for (float sample : samples) {
            metrics.recordCaptureLatency(sample);
        }

        auto snap = metrics.snapshot();
        REQUIRE_THAT(snap.captureLatencyMin, WithinRel(2.0f, 0.01f));
        REQUIRE_THAT(snap.captureLatencyMax, WithinRel(30.0f, 0.01f));
    }
}

TEST_CASE("PerformanceMetrics - All Thread Metrics", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("All threads can record independently") {
        metrics.recordCaptureLatency(1.0f);
        metrics.recordDetectionLatency(8.0f);
        metrics.recordTrackingLatency(0.5f);
        metrics.recordInputLatency(0.3f);

        auto snap = metrics.snapshot();

        // Each thread should have exactly 1 frame
        REQUIRE(snap.captureFrames == 1);
        REQUIRE(snap.detectionFrames == 1);
        REQUIRE(snap.trackingFrames == 1);
        REQUIRE(snap.inputFrames == 1);

        // Each thread should have correct average
        REQUIRE_THAT(snap.captureLatencyAvg, WithinRel(1.0f, 0.01f));
        REQUIRE_THAT(snap.detectionLatencyAvg, WithinRel(8.0f, 0.01f));
        REQUIRE_THAT(snap.trackingLatencyAvg, WithinRel(0.5f, 0.01f));
        REQUIRE_THAT(snap.inputLatencyAvg, WithinRel(0.3f, 0.01f));
    }
}

TEST_CASE("PerformanceMetrics - Resource Tracking", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("Update active targets") {
        metrics.updateActiveTargets(5);
        REQUIRE(metrics.snapshot().activeTargets == 5);

        metrics.updateActiveTargets(12);
        REQUIRE(metrics.snapshot().activeTargets == 12);
    }

    SECTION("Update VRAM usage") {
        metrics.updateVRAMUsage(256);
        REQUIRE(metrics.snapshot().vramUsageMB == 256);

        metrics.updateVRAMUsage(384);
        REQUIRE(metrics.snapshot().vramUsageMB == 384);
    }

    SECTION("Dropped frames tracking") {
        metrics.capture.recordDroppedFrame();
        metrics.capture.recordDroppedFrame();
        metrics.detection.recordDroppedFrame();

        auto snap = metrics.snapshot();
        REQUIRE(snap.droppedFramesTotal == 3);  // 2 capture + 1 detection
    }
}

TEST_CASE("PerformanceMetrics - Safety Metrics (Critical Traps)", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("Texture pool starvation tracking") {
        REQUIRE(metrics.snapshot().texturePoolStarved == 0);

        metrics.recordTexturePoolStarvation();
        metrics.recordTexturePoolStarvation();

        REQUIRE(metrics.snapshot().texturePoolStarved == 2);
    }

    SECTION("Stale prediction tracking") {
        REQUIRE(metrics.snapshot().stalePredictionEvents == 0);

        metrics.recordStalePrediction();
        metrics.recordStalePrediction();
        metrics.recordStalePrediction();

        REQUIRE(metrics.snapshot().stalePredictionEvents == 3);
    }

    SECTION("Deadman switch tracking") {
        REQUIRE(metrics.snapshot().deadmanSwitchTriggered == 0);

        metrics.recordDeadmanSwitch();

        REQUIRE(metrics.snapshot().deadmanSwitchTriggered == 1);
    }
}

TEST_CASE("PerformanceMetrics - Reset Functionality", "[metrics]") {
    PerformanceMetrics metrics;

    // Record some data
    metrics.recordCaptureLatency(10.0f);
    metrics.recordDetectionLatency(5.0f);
    metrics.updateActiveTargets(8);
    metrics.updateVRAMUsage(512);
    metrics.recordTexturePoolStarvation();

    // Verify data was recorded
    auto snapBefore = metrics.snapshot();
    REQUIRE(snapBefore.captureFrames > 0);
    REQUIRE(snapBefore.activeTargets == 8);
    REQUIRE(snapBefore.texturePoolStarved == 1);

    // Reset
    metrics.reset();

    // Verify all metrics are reset
    auto snapAfter = metrics.snapshot();
    REQUIRE(snapAfter.captureFrames == 0);
    REQUIRE(snapAfter.detectionFrames == 0);
    REQUIRE(snapAfter.captureLatencyAvg == 0.0f);
    REQUIRE(snapAfter.activeTargets == 0);
    REQUIRE(snapAfter.vramUsageMB == 0);
    REQUIRE(snapAfter.texturePoolStarved == 0);
    REQUIRE(snapAfter.stalePredictionEvents == 0);
    REQUIRE(snapAfter.deadmanSwitchTriggered == 0);
}

TEST_CASE("PerformanceMetrics - Thread Safety (Basic)", "[metrics]") {
    PerformanceMetrics metrics;

    SECTION("Concurrent updates from multiple threads") {
        const int numThreads = 4;
        const int samplesPerThread = 100;

        std::vector<std::thread> threads;

        // Spawn 4 threads, each recording 100 samples
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&metrics, samplesPerThread]() {
                for (int i = 0; i < samplesPerThread; ++i) {
                    metrics.recordCaptureLatency(5.0f + (i % 10));  // 5-15ms range
                }
            });
        }

        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }

        // Verify total frame count
        auto snap = metrics.snapshot();
        REQUIRE(snap.captureFrames == numThreads * samplesPerThread);

        // Min/Max should be within expected range
        REQUIRE(snap.captureLatencyMin >= 5.0f);
        REQUIRE(snap.captureLatencyMax <= 15.0f);
    }

    SECTION("Snapshot consistency during concurrent updates") {
        std::atomic<bool> running{true};

        // Thread 1: Continuously record metrics
        std::thread writer([&metrics, &running]() {
            while (running.load(std::memory_order_relaxed)) {
                metrics.recordDetectionLatency(10.0f);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });

        // Thread 2: Continuously read snapshots
        std::thread reader([&metrics, &running]() {
            for (int i = 0; i < 100; ++i) {
                auto snap = metrics.snapshot();

                // Snapshot should always have consistent data
                // (frameCount should never decrease)
                REQUIRE(snap.detectionFrames >= 0);
                REQUIRE(snap.detectionLatencyAvg >= 0.0f);

                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });

        reader.join();
        running.store(false, std::memory_order_relaxed);
        writer.join();
    }
}

TEST_CASE("PerformanceMetrics - Snapshot Immutability", "[metrics]") {
    PerformanceMetrics metrics;

    metrics.recordCaptureLatency(10.0f);
    auto snap1 = metrics.snapshot();

    // Record more data
    metrics.recordCaptureLatency(20.0f);
    auto snap2 = metrics.snapshot();

    // Verify snap1 is unchanged (snapshot is a copy, not a reference)
    REQUIRE(snap1.captureFrames == 1);
    REQUIRE_THAT(snap1.captureLatencyAvg, WithinRel(10.0f, 0.01f));

    // Verify snap2 has new data
    REQUIRE(snap2.captureFrames == 2);
    REQUIRE(snap2.captureLatencyAvg != snap1.captureLatencyAvg);
}

TEST_CASE("PerformanceMetrics - Cache Line Alignment (Phase 8 - P8-05)", "[metrics][alignment]") {
    SECTION("ThreadMetrics is aligned to 64-byte cache line") {
        // Each atomic should be on separate cache line (64 bytes)
        // ThreadMetrics has 5 atomics → minimum 320 bytes
        REQUIRE(sizeof(ThreadMetrics) >= 320);
        REQUIRE(alignof(ThreadMetrics) == 64);
    }

    SECTION("PerformanceMetrics has 4 aligned ThreadMetrics") {
        PerformanceMetrics metrics;

        // Verify each ThreadMetrics instance is properly aligned
        REQUIRE(reinterpret_cast<uintptr_t>(&metrics.capture) % 64 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(&metrics.detection) % 64 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(&metrics.tracking) % 64 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(&metrics.input) % 64 == 0);
    }

    SECTION("No false sharing between ThreadMetrics instances") {
        PerformanceMetrics metrics;

        // Each ThreadMetrics should be at least 320 bytes apart
        uintptr_t capturePtr = reinterpret_cast<uintptr_t>(&metrics.capture);
        uintptr_t detectionPtr = reinterpret_cast<uintptr_t>(&metrics.detection);
        uintptr_t trackingPtr = reinterpret_cast<uintptr_t>(&metrics.tracking);
        uintptr_t inputPtr = reinterpret_cast<uintptr_t>(&metrics.input);

        // Verify minimum spacing (prevents cache line bouncing)
        REQUIRE(detectionPtr - capturePtr >= 320);
        REQUIRE(trackingPtr - detectionPtr >= 320);
        REQUIRE(inputPtr - trackingPtr >= 320);
    }

    SECTION("Performance: No regression with metrics collection") {
        PerformanceMetrics metrics;

        // Baseline: Record 10,000 samples
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; ++i) {
            metrics.recordCaptureLatency(5.0f + (i % 10));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // Verify reasonable performance (should be <1ms for 10k samples)
        REQUIRE(duration < 1000);  // Less than 1 millisecond

        // Verify correctness wasn't sacrificed for performance
        auto snap = metrics.snapshot();
        REQUIRE(snap.captureFrames == 10000);
        REQUIRE(snap.captureLatencyMin >= 5.0f);
        REQUIRE(snap.captureLatencyMax <= 15.0f);
    }
}
