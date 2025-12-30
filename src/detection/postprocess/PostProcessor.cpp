#include "PostProcessor.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sunone {

PostProcessor::PostProcessor(const Config& config)
    : config_(config) {}

DetectionList PostProcessor::process(const float* output, int numDetections) {
    DetectionList detections;

    for (int i = 0; i < numDetections; ++i) {
        // YOLO format: x, y, w, h, conf, class_scores...
        int stride = 5 + static_cast<int>(config_.targetClasses.size());
        const float* det = output + i * stride;

        float confidence = det[4];
        if (confidence < config_.confidenceThreshold) {
            continue;
        }

        // Find best class
        int bestClass = 0;
        float bestScore = 0;
        for (size_t c = 0; c < config_.targetClasses.size(); ++c) {
            float score = det[5 + c] * confidence;
            if (score > bestScore) {
                bestScore = score;
                bestClass = config_.targetClasses[c];
            }
        }

        if (bestScore < config_.confidenceThreshold) {
            continue;
        }

        // Convert to box (center format to corner format)
        Detection d;
        float cx = det[0];
        float cy = det[1];
        float w = det[2];
        float h = det[3];

        d.box.x = static_cast<int>(cx - w / 2);
        d.box.y = static_cast<int>(cy - h / 2);
        d.box.width = static_cast<int>(w);
        d.box.height = static_cast<int>(h);
        d.confidence = bestScore;
        d.classId = bestClass;

        detections.push_back(d);
    }

    return applyNMS(detections, config_.nmsThreshold);
}

float PostProcessor::calculateIoU(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    int interWidth = std::max(0, x2 - x1);
    int interHeight = std::max(0, y2 - y1);
    int interArea = interWidth * interHeight;

    int unionArea = a.width * a.height + b.width * b.height - interArea;

    if (unionArea <= 0) {
        return 0.0f;
    }

    return static_cast<float>(interArea) / static_cast<float>(unionArea);
}

DetectionList PostProcessor::applyNMS(const DetectionList& detections, float threshold) {
    if (detections.empty()) {
        return {};
    }

    // Create indices and sort by confidence descending
    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return detections[a].confidence > detections[b].confidence;
    });

    std::vector<bool> suppressed(detections.size(), false);
    DetectionList result;
    result.reserve(detections.size());

    for (size_t i = 0; i < indices.size(); ++i) {
        size_t idx = indices[i];
        if (suppressed[idx]) {
            continue;
        }

        result.push_back(detections[idx]);
        const cv::Rect& boxA = detections[idx].box;

        for (size_t j = i + 1; j < indices.size(); ++j) {
            size_t jdx = indices[j];
            if (suppressed[jdx]) {
                continue;
            }

            const cv::Rect& boxB = detections[jdx].box;
            float iou = calculateIoU(boxA, boxB);

            if (iou > threshold) {
                suppressed[jdx] = true;
            }
        }
    }

    return result;
}

void PostProcessor::scaleToOriginal(DetectionList& detections,
                                    int modelWidth, int modelHeight,
                                    int imageWidth, int imageHeight) {
    if (modelWidth <= 0 || modelHeight <= 0) {
        return;
    }

    float scaleX = static_cast<float>(imageWidth) / static_cast<float>(modelWidth);
    float scaleY = static_cast<float>(imageHeight) / static_cast<float>(modelHeight);

    for (auto& det : detections) {
        det.box.x = static_cast<int>(det.box.x * scaleX);
        det.box.y = static_cast<int>(det.box.y * scaleY);
        det.box.width = static_cast<int>(det.box.width * scaleX);
        det.box.height = static_cast<int>(det.box.height * scaleY);
    }
}

} // namespace sunone
