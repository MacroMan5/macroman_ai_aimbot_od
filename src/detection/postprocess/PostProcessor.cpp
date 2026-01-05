#include "PostProcessor.h"
#include <algorithm>

namespace macroman {

void PostProcessor::applyNMS(std::vector<Detection>& detections, float iouThreshold) {
    if (detections.empty()) return;

    // Sort by confidence (descending)
    std::sort(detections.begin(), detections.end(), [](const Detection& a, const Detection& b) {
        return a.confidence > b.confidence;
    });

    // NMS loop
    std::vector<Detection> kept;
    kept.reserve(detections.size());

    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& current = detections[i];
        bool suppressed = false;

        // Check against all kept boxes
        for (const auto& keptBox : kept) {
            float iou = calculateIoU(current.bbox, keptBox.bbox);
            if (iou > iouThreshold) {
                suppressed = true;
                break;
            }
        }

        if (!suppressed) {
            kept.push_back(current);
        }
    }

    detections = std::move(kept);
}

void PostProcessor::filterByConfidence(std::vector<Detection>& detections, float minConfidence) {
    detections.erase(
        std::remove_if(detections.begin(), detections.end(), [minConfidence](const Detection& det) {
            return det.confidence < minConfidence;
        }),
        detections.end()
    );
}

void PostProcessor::mapHitboxes(
    std::vector<Detection>& detections,
    const std::unordered_map<int, HitboxType>& hitboxMapping
) {
    for (auto& det : detections) {
        auto it = hitboxMapping.find(det.classId);
        if (it != hitboxMapping.end()) {
            det.hitbox = it->second;
        } else {
            det.hitbox = HitboxType::Unknown;
        }
    }
}

float PostProcessor::calculateIoU(const BBox& a, const BBox& b) {
    // Calculate intersection
    float x1 = (std::max)(a.x, b.x);
    float y1 = (std::max)(a.y, b.y);
    float x2 = (std::min)(a.x + a.width, b.x + b.width);
    float y2 = (std::min)(a.y + a.height, b.y + b.height);

    float intersectionWidth = (std::max)(0.0f, x2 - x1);
    float intersectionHeight = (std::max)(0.0f, y2 - y1);
    float intersectionArea = intersectionWidth * intersectionHeight;

    // Calculate union
    float areaA = a.area();
    float areaB = b.area();
    float unionArea = areaA + areaB - intersectionArea;

    // Avoid division by zero
    if (unionArea <= 0.0f) {
        return 0.0f;
    }

    return intersectionArea / unionArea;
}

} // namespace macroman
