#pragma once

#include <atomic>
#include <cstdint>

namespace macroman {

/**
 * @brief Shared configuration for IPC (Engine <-> Config UI)
 *
 * Memory Layout:
 * - Cache-line aligned (64 bytes) to avoid false sharing
 * - Lock-free atomics ONLY (verified via static_assert)
 * - Hot-path tunables first (smoothness, FOV, enable flags)
 * - Telemetry fields second (FPS, latency, metrics)
 *
 * Thread Safety: Lock-free. All fields are std::atomic.
 * Platform Requirement: x64 Windows (atomics guaranteed lock-free)
 *
 * Architecture Reference: Critical Trap #3 (Shared Memory Atomics)
 *
 * Usage:
 * - Engine: Reads config every frame, writes telemetry
 * - Config UI: Writes config when user adjusts sliders, reads telemetry
 *
 * Lifecycle:
 * 1. Engine creates memory-mapped file
 * 2. Config UI opens existing mapping
 * 3. Both processes read/write atomics (no locks)
 * 4. Engine closes mapping on shutdown
 */
struct SharedConfig {
    //
    // HOT-PATH TUNABLES (Read by engine every frame)
    //

    alignas(64) std::atomic<float> aimSmoothness{0.5f};          // [0.0, 1.0] (0=instant, 1=very smooth)
    alignas(64) std::atomic<float> fov{80.0f};                   // Field of view (degrees)
    alignas(64) std::atomic<uint32_t> activeProfileId{0};        // Currently active game profile ID
    alignas(64) std::atomic<bool> enablePrediction{true};        // Enable Kalman prediction
    alignas(64) std::atomic<bool> enableAiming{true};            // Master enable for mouse movement
    alignas(64) std::atomic<bool> enableTracking{true};          // Enable target tracking
    alignas(64) std::atomic<bool> enableTremor{true};            // Enable humanization tremor

    char padding1[64];  // Avoid false sharing with telemetry

    //
    // TELEMETRY (Written by engine, read by Config UI)
    //

    alignas(64) std::atomic<float> captureFPS{0.0f};             // Frames/second from capture thread
    alignas(64) std::atomic<float> captureLatency{0.0f};         // Average capture latency (ms)
    alignas(64) std::atomic<float> detectionLatency{0.0f};       // Average detection latency (ms)
    alignas(64) std::atomic<float> trackingLatency{0.0f};        // Average tracking latency (ms)
    alignas(64) std::atomic<float> inputLatency{0.0f};           // Average input latency (ms)
    alignas(64) std::atomic<int> activeTargets{0};               // Number of tracked targets
    alignas(64) std::atomic<size_t> vramUsageMB{0};              // VRAM usage in megabytes

    char padding2[64];  // Avoid false sharing with safety metrics

    //
    // SAFETY METRICS (Critical Trap tracking)
    //

    alignas(64) std::atomic<uint64_t> texturePoolStarved{0};     // Trap 1: Pool starvation events
    alignas(64) std::atomic<uint64_t> stalePredictionEvents{0};  // Trap 2: >50ms extrapolation count
    alignas(64) std::atomic<uint64_t> deadmanSwitchTriggered{0}; // Trap 4: Safety trigger count

    /**
     * @brief Reset all fields to default values
     *
     * Called by engine on initialization before creating mapping.
     */
    void reset() {
        // Config defaults
        aimSmoothness.store(0.5f, std::memory_order_relaxed);
        fov.store(80.0f, std::memory_order_relaxed);
        activeProfileId.store(0, std::memory_order_relaxed);
        enablePrediction.store(true, std::memory_order_relaxed);
        enableAiming.store(true, std::memory_order_relaxed);
        enableTracking.store(true, std::memory_order_relaxed);
        enableTremor.store(true, std::memory_order_relaxed);

        // Telemetry defaults
        captureFPS.store(0.0f, std::memory_order_relaxed);
        captureLatency.store(0.0f, std::memory_order_relaxed);
        detectionLatency.store(0.0f, std::memory_order_relaxed);
        trackingLatency.store(0.0f, std::memory_order_relaxed);
        inputLatency.store(0.0f, std::memory_order_relaxed);
        activeTargets.store(0, std::memory_order_relaxed);
        vramUsageMB.store(0, std::memory_order_relaxed);

        // Safety metrics
        texturePoolStarved.store(0, std::memory_order_relaxed);
        stalePredictionEvents.store(0, std::memory_order_relaxed);
        deadmanSwitchTriggered.store(0, std::memory_order_relaxed);
    }
};

//
// COMPILE-TIME VERIFICATION (Critical Trap #3)
//

// Ensure all atomics are lock-free for IPC safety
static_assert(std::atomic<float>::is_always_lock_free,
              "std::atomic<float> MUST be lock-free for IPC safety");
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "std::atomic<uint32_t> MUST be lock-free for IPC safety");
static_assert(std::atomic<bool>::is_always_lock_free,
              "std::atomic<bool> MUST be lock-free for IPC safety");
static_assert(std::atomic<int>::is_always_lock_free,
              "std::atomic<int> MUST be lock-free for IPC safety");
static_assert(std::atomic<size_t>::is_always_lock_free,
              "std::atomic<size_t> MUST be lock-free for IPC safety");
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "std::atomic<uint64_t> MUST be lock-free for IPC safety");

// Verify alignment consistency across processes
static_assert(alignof(SharedConfig) == 64,
              "SharedConfig alignment mismatch between processes");

/**
 * @brief Non-atomic snapshot of SharedConfig for safe reading
 *
 * UI threads should call snapshot() to get a consistent copy without
 * lock contention or tearing.
 */
struct ConfigSnapshot {
    // Config
    float aimSmoothness{0.5f};
    float fov{80.0f};
    uint32_t activeProfileId{0};
    bool enablePrediction{true};
    bool enableAiming{true};
    bool enableTracking{true};
    bool enableTremor{true};

    // Telemetry
    float captureFPS{0.0f};
    float captureLatency{0.0f};
    float detectionLatency{0.0f};
    float trackingLatency{0.0f};
    float inputLatency{0.0f};
    int activeTargets{0};
    size_t vramUsageMB{0};

    // Safety metrics
    uint64_t texturePoolStarved{0};
    uint64_t stalePredictionEvents{0};
    uint64_t deadmanSwitchTriggered{0};

    /**
     * @brief Create snapshot from atomic SharedConfig
     *
     * All loads use memory_order_relaxed (not synchronized across fields).
     * Individual fields are consistent, but not guaranteed to be from
     * the same instant in time.
     */
    static ConfigSnapshot snapshot(const SharedConfig& config) {
        ConfigSnapshot snap;

        // Config
        snap.aimSmoothness = config.aimSmoothness.load(std::memory_order_relaxed);
        snap.fov = config.fov.load(std::memory_order_relaxed);
        snap.activeProfileId = config.activeProfileId.load(std::memory_order_relaxed);
        snap.enablePrediction = config.enablePrediction.load(std::memory_order_relaxed);
        snap.enableAiming = config.enableAiming.load(std::memory_order_relaxed);
        snap.enableTracking = config.enableTracking.load(std::memory_order_relaxed);
        snap.enableTremor = config.enableTremor.load(std::memory_order_relaxed);

        // Telemetry
        snap.captureFPS = config.captureFPS.load(std::memory_order_relaxed);
        snap.captureLatency = config.captureLatency.load(std::memory_order_relaxed);
        snap.detectionLatency = config.detectionLatency.load(std::memory_order_relaxed);
        snap.trackingLatency = config.trackingLatency.load(std::memory_order_relaxed);
        snap.inputLatency = config.inputLatency.load(std::memory_order_relaxed);
        snap.activeTargets = config.activeTargets.load(std::memory_order_relaxed);
        snap.vramUsageMB = config.vramUsageMB.load(std::memory_order_relaxed);

        // Safety metrics
        snap.texturePoolStarved = config.texturePoolStarved.load(std::memory_order_relaxed);
        snap.stalePredictionEvents = config.stalePredictionEvents.load(std::memory_order_relaxed);
        snap.deadmanSwitchTriggered = config.deadmanSwitchTriggered.load(std::memory_order_relaxed);

        return snap;
    }
};

} // namespace macroman
