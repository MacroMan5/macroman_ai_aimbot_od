#pragma once

#include "../../core/entities/Detection.h"
#include <vector>

namespace macroman {

/**
 * Post-processing utilities for YOLO model outputs.
 * Provides NMS (Non-Maximum Suppression) and scaling operations.
 */
class PostProcessor {
public:
    struct Config {
        float confidenceThreshold = 0.25f;
        float nmsThreshold = 0.45f;
        std::vector<int> targetClasses = {0};
        int inputWidth = 640;
        int inputHeight = 640;
    };

    explicit PostProcessor(const Config& config);
    PostProcessor() : PostProcessor(Config{}) {}

    /**
     * Process raw model output into detections.
     * @param output Raw float output from model
     * @param numDetections Number of detections in output
     * @return List of filtered detections after confidence filtering and NMS
     */
    DetectionList process(const float* output, int numDetections);

    /**
     * Apply Non-Maximum Suppression to remove overlapping detections.
     * @param detections Input detections (will not be modified)
     * @param threshold IoU threshold above which detections are suppressed
     * @return Filtered detection list
     */
    static DetectionList applyNMS(const DetectionList& detections, float threshold);

    /**
     * Scale detection boxes from model coordinates to original image coordinates.
     * @param detections Detections to scale (modified in place)
     * @param modelWidth Model input width
     * @param modelHeight Model input height
     * @param imageWidth Original image width
     * @param imageHeight Original image height
     */
    static void scaleToOriginal(DetectionList& detections,
                                int modelWidth, int modelHeight,
                                int imageWidth, int imageHeight);

    /**
     * Calculate Intersection over Union (IoU) between two boxes.
     * @param a First bounding box
     * @param b Second bounding box
     * @return IoU value in range [0, 1]
     */
    static float calculateIoU(const cv::Rect& a, const cv::Rect& b);

    void setConfig(const Config& config) { config_ = config; }
    const Config& getConfig() const { return config_; }

private:
    Config config_;
};

} // namespace macroman
