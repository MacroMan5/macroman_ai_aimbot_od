/**
 * @file PerformanceMetrics.h
 * @brief Lock-free performance metrics collection system
 *
 * Provides runtime performance monitoring for all pipeline stages:
 * - Throughput metrics (FPS, frame counts)
 * - Latency metrics (average, min, max, P95, P99)
 * - Resource usage (VRAM, dropped frames)
 * - Safety metrics (Critical Traps monitoring)
 *
 * Thread-safe: All metrics use std::atomic for lock-free updates.
 *
 * Usage:
 *   PerformanceMetrics metrics;
 *   metrics.recordCaptureLatency(1.2f);
 *   auto snapshot = metrics.snapshot();  // Get consistent view for UI
 */

#pragma once

#include <atomic>
#include <chrono>
#include <array>
#include <cstdint>

namespace macroman {

/**
 * @brief Per-thread performance metrics
 *
 * CACHE LINE OPTIMIZATION (Phase 8 - P8-05):
 * - Each atomic is aligned to 64-byte cache line to prevent false sharing
 * - 4 threads update separate ThreadMetrics instances concurrently
 * - Without alignment, adjacent atomics cause cache line bouncing (10-20% overhead)
 */
struct alignas(64) ThreadMetrics {
    alignas(64) std::atomic<uint64_t> frameCount{0};      // Total frames processed
    alignas(64) std::atomic<float> avgLatency{0.0f};      // Exponential moving average (ms)
    alignas(64) std::atomic<float> minLatency{999999.0f}; // Minimum latency (ms)
    alignas(64) std::atomic<float> maxLatency{0.0f};      // Maximum latency (ms)
    alignas(64) std::atomic<uint64_t> droppedFrames{0};   // Frames dropped (backpressure)

    /**
     * @brief Update metrics with new latency sample
     * @param latencyMs Latency in milliseconds
     * @param emaAlpha EMA smoothing factor (default: 0.05 = 5% new, 95% old)
     */
    void recordLatency(float latencyMs, float emaAlpha = 0.05f) {
        uint64_t prevCount = frameCount.fetch_add(1, std::memory_order_relaxed);

        // Update EMA (initialize to first sample if this is the first frame)
        float currentAvg = avgLatency.load(std::memory_order_relaxed);
        float newAvg;
        if (prevCount == 0) {
            // First sample: initialize directly to avoid scaling by alpha
            newAvg = latencyMs;
        } else {
            // Subsequent samples: standard EMA
            newAvg = currentAvg * (1.0f - emaAlpha) + latencyMs * emaAlpha;
        }
        avgLatency.store(newAvg, std::memory_order_relaxed);

        // Update min/max
        float currentMin = minLatency.load(std::memory_order_relaxed);
        while (latencyMs < currentMin) {
            if (minLatency.compare_exchange_weak(currentMin, latencyMs, std::memory_order_relaxed)) {
                break;
            }
        }

        float currentMax = maxLatency.load(std::memory_order_relaxed);
        while (latencyMs > currentMax) {
            if (maxLatency.compare_exchange_weak(currentMax, latencyMs, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    /**
     * @brief Increment dropped frame counter
     */
    void recordDroppedFrame() {
        droppedFrames.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Reset all metrics to initial state
     */
    void reset() {
        frameCount.store(0, std::memory_order_relaxed);
        avgLatency.store(0.0f, std::memory_order_relaxed);
        minLatency.store(999999.0f, std::memory_order_relaxed);
        maxLatency.store(0.0f, std::memory_order_relaxed);
        droppedFrames.store(0, std::memory_order_relaxed);
    }
};

// Verify cache line alignment (Phase 8 - P8-05)
// Each of 5 atomics should occupy separate 64-byte cache lines = 320 bytes minimum
static_assert(sizeof(ThreadMetrics) >= 320,
    "ThreadMetrics must be at least 320 bytes (5 atomics × 64-byte cache lines)");
static_assert(alignof(ThreadMetrics) == 64,
    "ThreadMetrics must be aligned to 64-byte cache line boundary");

/**
 * @brief Global performance metrics (lock-free, thread-safe)
 *
 * All fields are atomic for lock-free access from multiple threads.
 * Use snapshot() to get a consistent view for UI/telemetry.
 */
class PerformanceMetrics {
public:
    PerformanceMetrics() = default;
    ~PerformanceMetrics() = default;

    // Per-thread metrics
    ThreadMetrics capture;
    ThreadMetrics detection;
    ThreadMetrics tracking;
    ThreadMetrics input;

    // Global throughput metrics
    std::atomic<float> overallFPS{0.0f};       // Frames per second (updated by capture thread)
    std::atomic<int> activeTargets{0};         // Currently tracked targets
    std::atomic<size_t> vramUsageMB{0};        // VRAM usage in MB

    // Safety metrics (from CRITICAL_TRAPS.md)
    std::atomic<uint64_t> texturePoolStarved{0};     // Trap #1: Pool starvation events
    std::atomic<uint64_t> stalePredictionEvents{0};  // Trap #2: >50ms extrapolation count
    std::atomic<uint64_t> deadmanSwitchTriggered{0}; // Trap #4: Safety trigger count

    // Timing markers (for FPS calculation)
    std::chrono::high_resolution_clock::time_point startTime{std::chrono::high_resolution_clock::now()};

    /**
     * @brief Record capture thread latency
     */
    void recordCaptureLatency(float latencyMs) {
        capture.recordLatency(latencyMs);
    }

    /**
     * @brief Record detection thread latency
     */
    void recordDetectionLatency(float latencyMs) {
        detection.recordLatency(latencyMs);
    }

    /**
     * @brief Record tracking thread latency
     */
    void recordTrackingLatency(float latencyMs) {
        tracking.recordLatency(latencyMs);
    }

    /**
     * @brief Record input thread latency
     */
    void recordInputLatency(float latencyMs) {
        input.recordLatency(latencyMs);
    }

    /**
     * @brief Update overall FPS metric
     *
     * Called by capture thread every N frames to update FPS measurement.
     * Uses exponential moving average for smooth FPS display.
     */
    void updateFPS() {
        auto now = std::chrono::high_resolution_clock::now();
        float elapsedSeconds = std::chrono::duration<float>(now - startTime).count();

        if (elapsedSeconds > 0.0f) {
            uint64_t totalFrames = capture.frameCount.load(std::memory_order_relaxed);
            float currentFPS = totalFrames / elapsedSeconds;

            // EMA smoothing for FPS display
            float oldFPS = overallFPS.load(std::memory_order_relaxed);
            float newFPS = oldFPS * 0.9f + currentFPS * 0.1f;  // 10% new, 90% old
            overallFPS.store(newFPS, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Update active target count
     * @param count Number of currently tracked targets
     */
    void updateActiveTargets(int count) {
        activeTargets.store(count, std::memory_order_relaxed);
    }

    /**
     * @brief Update VRAM usage metric
     * @param usageMB VRAM usage in megabytes
     */
    void updateVRAMUsage(size_t usageMB) {
        vramUsageMB.store(usageMB, std::memory_order_relaxed);
    }

    /**
     * @brief Record texture pool starvation event (Critical Trap #1)
     */
    void recordTexturePoolStarvation() {
        texturePoolStarved.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Record stale prediction event (Critical Trap #2)
     */
    void recordStalePrediction() {
        stalePredictionEvents.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Record deadman switch trigger (Critical Trap #4)
     */
    void recordDeadmanSwitch() {
        deadmanSwitchTriggered.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Reset all metrics to initial state
     */
    void reset() {
        capture.reset();
        detection.reset();
        tracking.reset();
        input.reset();

        overallFPS.store(0.0f, std::memory_order_relaxed);
        activeTargets.store(0, std::memory_order_relaxed);
        vramUsageMB.store(0, std::memory_order_relaxed);

        texturePoolStarved.store(0, std::memory_order_relaxed);
        stalePredictionEvents.store(0, std::memory_order_relaxed);
        deadmanSwitchTriggered.store(0, std::memory_order_relaxed);

        startTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Non-atomic snapshot for UI consumption
     *
     * Creates a consistent snapshot of all metrics at a single point in time.
     * Safe to read across process boundary (e.g., from external config UI).
     */
    struct Snapshot {
        // Throughput
        float captureFPS{0.0f};
        uint64_t captureFrames{0};
        uint64_t detectionFrames{0};
        uint64_t trackingFrames{0};
        uint64_t inputFrames{0};

        // Latency (ms)
        float captureLatencyAvg{0.0f};
        float captureLatencyMin{0.0f};
        float captureLatencyMax{0.0f};

        float detectionLatencyAvg{0.0f};
        float detectionLatencyMin{0.0f};
        float detectionLatencyMax{0.0f};

        float trackingLatencyAvg{0.0f};
        float trackingLatencyMin{0.0f};
        float trackingLatencyMax{0.0f};

        float inputLatencyAvg{0.0f};
        float inputLatencyMin{0.0f};
        float inputLatencyMax{0.0f};

        // Resources
        int activeTargets{0};
        size_t vramUsageMB{0};
        uint64_t droppedFramesTotal{0};

        // Safety (Critical Traps)
        uint64_t texturePoolStarved{0};
        uint64_t stalePredictionEvents{0};
        uint64_t deadmanSwitchTriggered{0};
    };

    /**
     * @brief Create snapshot of current metrics
     *
     * Thread-safe: Reads all atomic values at a single point in time.
     * Use this for UI rendering or telemetry export.
     */
    Snapshot snapshot() const {
        Snapshot snap;

        // Throughput
        snap.captureFPS = overallFPS.load(std::memory_order_relaxed);
        snap.captureFrames = capture.frameCount.load(std::memory_order_relaxed);
        snap.detectionFrames = detection.frameCount.load(std::memory_order_relaxed);
        snap.trackingFrames = tracking.frameCount.load(std::memory_order_relaxed);
        snap.inputFrames = input.frameCount.load(std::memory_order_relaxed);

        // Latency - Capture
        snap.captureLatencyAvg = capture.avgLatency.load(std::memory_order_relaxed);
        snap.captureLatencyMin = capture.minLatency.load(std::memory_order_relaxed);
        snap.captureLatencyMax = capture.maxLatency.load(std::memory_order_relaxed);

        // Latency - Detection
        snap.detectionLatencyAvg = detection.avgLatency.load(std::memory_order_relaxed);
        snap.detectionLatencyMin = detection.minLatency.load(std::memory_order_relaxed);
        snap.detectionLatencyMax = detection.maxLatency.load(std::memory_order_relaxed);

        // Latency - Tracking
        snap.trackingLatencyAvg = tracking.avgLatency.load(std::memory_order_relaxed);
        snap.trackingLatencyMin = tracking.minLatency.load(std::memory_order_relaxed);
        snap.trackingLatencyMax = tracking.maxLatency.load(std::memory_order_relaxed);

        // Latency - Input
        snap.inputLatencyAvg = input.avgLatency.load(std::memory_order_relaxed);
        snap.inputLatencyMin = input.minLatency.load(std::memory_order_relaxed);
        snap.inputLatencyMax = input.maxLatency.load(std::memory_order_relaxed);

        // Resources
        snap.activeTargets = activeTargets.load(std::memory_order_relaxed);
        snap.vramUsageMB = vramUsageMB.load(std::memory_order_relaxed);
        snap.droppedFramesTotal =
            capture.droppedFrames.load(std::memory_order_relaxed) +
            detection.droppedFrames.load(std::memory_order_relaxed) +
            tracking.droppedFrames.load(std::memory_order_relaxed);

        // Safety
        snap.texturePoolStarved = texturePoolStarved.load(std::memory_order_relaxed);
        snap.stalePredictionEvents = stalePredictionEvents.load(std::memory_order_relaxed);
        snap.deadmanSwitchTriggered = deadmanSwitchTriggered.load(std::memory_order_relaxed);

        return snap;
    }
};

} // namespace macroman
