#pragma once

#include <vector>
#include "core/entities/TargetDatabase.h"
#include "core/entities/Detection.h"

namespace macroman {

/**
 * @brief Data association algorithms for matching detections to existing targets
 *
 * ALGORITHM: Greedy IoU matching (simple, fast, sufficient for MVP)
 */
class DataAssociation {
public:
    struct Match {
        size_t detectionIndex;  // Index in detections vector
        size_t targetIndex;     // Index in TargetDatabase
        float iou;              // Intersection-over-Union score
    };

    struct AssociationResult {
        std::vector<Match> matches;                 // Successfully matched pairs
        std::vector<size_t> unmatchedDetections;    // New targets to add
        std::vector<size_t> unmatchedTargets;       // Lost targets (to be checked for grace period)
    };

    [[nodiscard]] static float computeIoU(const BBox& a, const BBox& b) noexcept;

    [[nodiscard]] static AssociationResult matchDetections(
        const TargetDatabase& targets,
        const std::vector<Detection>& detections,
        float iouThreshold = 0.3f
    ) noexcept;
};

} // namespace macroman
