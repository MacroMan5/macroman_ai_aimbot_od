#pragma once

#include <vector>
#include <cstdint>
#include "Detection.h"

namespace macroman {

/**
 * @brief Batch of detections from a single frame
 *
 * CRITICAL: Using std::vector with reserved capacity instead of FixedCapacityVector
 * for MVP simplicity. Pre-allocate to MAX_DETECTIONS to avoid reallocation in hot path.
 *
 * TODO Phase 8: Replace with FixedCapacityVector for zero-allocation guarantee.
 *
 * Data Flow:
 *   Detection Thread → creates DetectionBatch → pushes to LatestFrameQueue
 *   Tracking Thread → pops DetectionBatch → processes → deletes
 */
struct DetectionBatch {
    static constexpr size_t MAX_DETECTIONS = 64;

    std::vector<Detection> observations;  // Pre-allocated to MAX_DETECTIONS
    uint64_t frameSequence{0};            // Corresponds to Frame::frameSequence
    int64_t captureTimeNs{0};             // Timestamp from Frame::captureTimeNs

    DetectionBatch() {
        observations.reserve(MAX_DETECTIONS);  // Pre-allocate to avoid reallocation
    }

    // Move-only (no copies in hot path)
    DetectionBatch(const DetectionBatch&) = delete;
    DetectionBatch& operator=(const DetectionBatch&) = delete;
    DetectionBatch(DetectionBatch&&) noexcept = default;
    DetectionBatch& operator=(DetectionBatch&&) noexcept = default;

    [[nodiscard]] bool isEmpty() const noexcept {
        return observations.empty();
    }
};

} // namespace macroman
