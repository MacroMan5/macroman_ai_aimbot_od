#pragma once

/**
 * SPECIFICATION: PostProcessor
 * =============================
 *
 * PURPOSE:
 * Post-process YOLO model outputs:
 * 1. Non-Maximum Suppression (NMS) - Remove overlapping detections
 * 2. Confidence filtering - Remove low-confidence detections
 * 3. Hitbox mapping - Map classId to HitboxType
 *
 * NMS ALGORITHM:
 * - IoU (Intersection over Union) threshold: 0.45 (from architecture doc)
 * - Sort by confidence (descending)
 * - Greedily select highest-confidence boxes, suppress overlapping ones
 *
 * HITBOX MAPPING (per-game configuration):
 * - classId 0 → Head (highest priority)
 * - classId 1 → Chest (medium priority)
 * - classId 2 → Body (lowest priority)
 *
 * PERFORMANCE TARGET:
 * - <1ms for typical detections (5-10 boxes)
 */

#include "core/entities/Detection.h"
#include <vector>
#include <unordered_map>

namespace macroman {

class PostProcessor {
public:
    /**
     * @brief Apply Non-Maximum Suppression
     * @param detections Input detections (will be modified in-place)
     * @param iouThreshold IoU threshold for suppression (default: 0.45)
     *
     * Modifies detections vector to remove suppressed boxes.
     */
    static void applyNMS(std::vector<Detection>& detections, float iouThreshold = 0.45f);

    /**
     * @brief Filter detections by confidence threshold
     * @param detections Input detections (will be modified in-place)
     * @param minConfidence Minimum confidence (default: 0.6)
     */
    static void filterByConfidence(std::vector<Detection>& detections, float minConfidence = 0.6f);

    /**
     * @brief Map class IDs to hitbox types
     * @param detections Input detections (will be modified in-place)
     * @param hitboxMapping Map from classId to HitboxType
     */
    static void mapHitboxes(
        std::vector<Detection>& detections,
        const std::unordered_map<int, HitboxType>& hitboxMapping
    );

private:
    // Helper: Calculate Intersection over Union
    static float calculateIoU(const BBox& a, const BBox& b);
};

} // namespace macroman
