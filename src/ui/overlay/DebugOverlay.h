#pragma once

#include "../../core/config/SharedConfig.h"
#include "../../core/entities/Detection.h"
#include "../../core/entities/MathTypes.h"
#include "FrameProfiler.h"
#include <imgui.h>
#include <Windows.h>
#include <array>
#include <chrono>
#include <string>

namespace macroman {

/**
 * @brief Snapshot of target bounding boxes for visualization
 *
 * Non-atomic copy of TargetDatabase state, safe for UI thread to read.
 * Updated by Tracking thread periodically (not every frame to reduce overhead).
 */
struct TargetSnapshot {
    static constexpr size_t MAX_TARGETS = 64;

    std::array<BBox, MAX_TARGETS> bboxes{};
    std::array<float, MAX_TARGETS> confidences{};
    std::array<HitboxType, MAX_TARGETS> hitboxTypes{};
    size_t count{0};
    size_t selectedTargetIndex{SIZE_MAX};  // Selected by Tracking thread

    void clear() {
        count = 0;
        selectedTargetIndex = SIZE_MAX;
    }
};

/**
 * @brief Non-atomic telemetry snapshot for UI consumption
 *
 * Provides consistent snapshot of atomic Metrics without lock contention.
 * Safe to read across process boundary.
 */
struct TelemetrySnapshot {
    float captureFPS{0.0f};
    float captureLatency{0.0f};
    float detectionLatency{0.0f};
    float trackingLatency{0.0f};
    float inputLatency{0.0f};
    float endToEndLatency{0.0f};  // Sum of all stages

    int activeTargets{0};
    size_t vramUsageMB{0};

    // Safety metrics (from CRITICAL_TRAPS.md)
    uint64_t texturePoolStarved{0};
    uint64_t stalePredictionEvents{0};
    uint64_t deadmanSwitchTriggered{0};

    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief In-game debug overlay for performance metrics and visualization
 *
 * Features:
 * - Transparent, frameless overlay window
 * - Screenshot protection (SetWindowDisplayAffinity)
 * - Real-time performance metrics
 * - Bounding box visualization (colored by hitbox type)
 * - Component toggles (enable/disable tracking, prediction, etc.)
 * - Minimal performance impact (<1ms per frame)
 *
 * Thread safety:
 * - UI thread reads telemetry snapshots (atomic loads)
 * - Tracking thread updates targetSnapshot (no locks, single writer)
 */
class DebugOverlay {
public:
    struct Config {
        bool enabled{true};
        bool showMetrics{true};
        bool showBBoxes{true};
        bool showComponentToggles{true};
        bool showSafetyMetrics{false};  // Hidden by default (advanced users)
        bool showProfiler{true};  // Show latency graphs

        float overlayAlpha{0.9f};  // 0.0 = fully transparent, 1.0 = opaque
        ImVec2 position{10.0f, 10.0f};  // Screen position (top-left corner)
    };

    DebugOverlay();
    ~DebugOverlay();

    /**
     * @brief Initialize overlay window with screenshot protection
     *
     * @param hwnd Parent window handle (game window)
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     * @return true if initialization succeeded
     */
    bool initialize(HWND hwnd, int screenWidth, int screenHeight);

    /**
     * @brief Shutdown overlay and cleanup resources
     */
    void shutdown();

    /**
     * @brief Render overlay (call from UI thread)
     *
     * Displays:
     * - Performance metrics (FPS, latency, VRAM)
     * - Bounding boxes (if enabled)
     * - Component toggles
     * - Safety metrics (if enabled)
     *
     * @param telemetry Current telemetry snapshot
     * @param targets Current target snapshot (for bounding boxes)
     * @param sharedConfig Pointer to shared config for component toggles
     */
    void render(const TelemetrySnapshot& telemetry,
                const TargetSnapshot& targets,
                SharedConfig* sharedConfig);

    /**
     * @brief Update configuration
     */
    void updateConfig(const Config& newConfig) { config_ = newConfig; }

    /**
     * @brief Get current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Check if overlay is initialized
     */
    bool isInitialized() const { return initialized_; }

private:
    /**
     * @brief Enable screenshot protection for overlay window
     *
     * Makes overlay invisible to:
     * - OBS recordings
     * - Discord screen share
     * - Anti-cheat screenshots
     * - Windows.Graphics.Capture API
     *
     * Requires Windows 10 1903+
     */
    void enableScreenshotProtection();

    /**
     * @brief Render performance metrics panel
     */
    void renderMetricsPanel(const TelemetrySnapshot& telemetry);

    /**
     * @brief Render bounding boxes on screen
     *
     * Colors:
     * - Selected target: GREEN
     * - Head: RED
     * - Chest: ORANGE
     * - Body: YELLOW
     * - Unknown: GRAY
     */
    void renderBoundingBoxes(const TargetSnapshot& targets);

    /**
     * @brief Render component toggle panel
     */
    void renderComponentToggles(SharedConfig* sharedConfig);

    /**
     * @brief Render safety metrics panel (advanced)
     */
    void renderSafetyMetrics(const TelemetrySnapshot& telemetry);

    /**
     * @brief Render frame profiler panel (latency graphs)
     */
    void renderProfilerPanel(const TelemetrySnapshot& telemetry);

    /**
     * @brief Get color for hitbox type
     */
    ImU32 getHitboxColor(HitboxType type) const;

    /**
     * @brief Format latency value with color coding
     *
     * Colors:
     * - Green: < 5ms (excellent)
     * - Yellow: 5-10ms (good)
     * - Red: > 10ms (poor)
     */
    ImVec4 getLatencyColor(float latencyMs) const;

    Config config_;
    bool initialized_{false};

    HWND overlayWindow_{nullptr};
    int screenWidth_{0};
    int screenHeight_{0};

    // Style caching
    ImFont* defaultFont_{nullptr};
    ImFont* metricsFont_{nullptr};

    // Frame profiler
    FrameProfiler profiler_;
};

} // namespace macroman
