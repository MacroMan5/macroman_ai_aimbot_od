#include "DataAssociation.h"
#include <algorithm>
#include <limits>

namespace macroman {

float DataAssociation::computeIoU(const BBox& a, const BBox& b) noexcept {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);

    if (x2 <= x1 || y2 <= y1) return 0.0f;

    float intersectionArea = (x2 - x1) * (y2 - y1);
    float unionArea = a.area() + b.area() - intersectionArea;

    if (unionArea < 1e-6f) return 0.0f;
    return intersectionArea / unionArea;
}

DataAssociation::AssociationResult DataAssociation::matchDetections(
    const TargetDatabase& targets,
    const std::vector<Detection>& detections,
    float iouThreshold
) noexcept {
    AssociationResult result;
    const size_t numTargets = targets.count;
    const size_t numDetections = detections.size();

    std::vector<bool> detectionMatched(numDetections, false);
    std::vector<bool> targetMatched(numTargets, false);

    // Greedy matching: repeatedly find best IoU pair above threshold
    while (true) {
        float bestIoU = iouThreshold;
        size_t bestDetectionIdx = SIZE_MAX;
        size_t bestTargetIdx = SIZE_MAX;

        // Find best unmatched pair
        for (size_t t = 0; t < numTargets; ++t) {
            if (targetMatched[t]) continue;
            for (size_t d = 0; d < numDetections; ++d) {
                if (detectionMatched[d]) continue;
                float iou = computeIoU(targets.bboxes[t], detections[d].bbox);
                if (iou > bestIoU) {
                    bestIoU = iou;
                    bestDetectionIdx = d;
                    bestTargetIdx = t;
                }
            }
        }

        // No more matches above threshold
        if (bestDetectionIdx == SIZE_MAX) break;

        // Record match
        result.matches.push_back({bestDetectionIdx, bestTargetIdx, bestIoU});
        detectionMatched[bestDetectionIdx] = true;
        targetMatched[bestTargetIdx] = true;
    }

    // Collect unmatched detections (new targets)
    for (size_t d = 0; d < numDetections; ++d) {
        if (!detectionMatched[d]) result.unmatchedDetections.push_back(d);
    }

    // Collect unmatched targets (lost tracks)
    for (size_t t = 0; t < numTargets; ++t) {
        if (!targetMatched[t]) result.unmatchedTargets.push_back(t);
    }

    return result;
}

} // namespace macroman
