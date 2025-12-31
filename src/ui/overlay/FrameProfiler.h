#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <imgui.h>

namespace macroman {

/**
 * @brief Frame performance profiler for latency visualization
 *
 * Tracks latency history for each pipeline stage and provides
 * bottleneck detection with suggestions.
 *
 * Features:
 * - Historical data storage (300 samples = 5 seconds @ 60fps UI refresh)
 * - Line graphs for each pipeline stage
 * - Bottleneck detection with thresholds
 * - Performance improvement suggestions
 *
 * Architecture:
 * - Ring buffer for efficient sample storage
 * - O(1) insertion, O(n) visualization
 * - Owned by DebugOverlay, updated every frame
 */
class FrameProfiler {
public:
    // Constants (must be declared before use in function signatures)
    static constexpr size_t HISTORY_SIZE = 300;  ///< 5 seconds @ 60fps UI refresh

    /**
     * @brief Update latency metrics for current frame
     *
     * @param captureMs Capture stage latency (ms)
     * @param detectionMs Detection stage latency (ms)
     * @param trackingMs Tracking stage latency (ms)
     * @param inputMs Input stage latency (ms)
     */
    void update(float captureMs, float detectionMs, float trackingMs, float inputMs);

    /**
     * @brief Render latency graphs and bottleneck suggestions
     *
     * Displays:
     * - 4 line graphs (one per pipeline stage)
     * - Average latency for each stage
     * - Bottleneck warnings (if any stage exceeds threshold)
     * - Performance improvement suggestions
     */
    void renderGraphs();

    /**
     * @brief Reset all historical data
     */
    void reset();

private:
    /**
     * @brief Detect bottleneck stage and provide suggestions
     *
     * Uses fixed thresholds per stage (from architecture doc):
     * - Capture: >2ms (P95 threshold)
     * - Detection: >10ms (P95 threshold)
     * - Tracking: >2ms (P95 threshold)
     * - Input: >1ms (P95 threshold)
     *
     * @return Bottleneck description and suggestion (empty if no bottleneck)
     */
    std::string detectBottleneck() const;

    /**
     * @brief Calculate average latency for a history buffer
     *
     * @param history Circular buffer of latency samples
     * @return Average latency (ms)
     */
    float calculateAverage(const std::array<float, HISTORY_SIZE>& history) const;

    // Historical data (ring buffers)
    std::array<float, HISTORY_SIZE> captureHistory_{};
    std::array<float, HISTORY_SIZE> detectionHistory_{};
    std::array<float, HISTORY_SIZE> trackingHistory_{};
    std::array<float, HISTORY_SIZE> inputHistory_{};

    size_t writeIndex_{0};  ///< Current write position in ring buffer
    bool bufferFilled_{false};  ///< True once we've wrapped around (for avg calculation)

    // Cached averages (updated on each update())
    float avgCapture_{0.0f};
    float avgDetection_{0.0f};
    float avgTracking_{0.0f};
    float avgInput_{0.0f};
};

}  // namespace macroman
