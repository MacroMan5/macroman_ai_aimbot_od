#include "FakeDetector.h"
#include <thread>
#include <chrono>
#include <algorithm>

namespace macroman {
namespace test {

bool FakeDetector::initialize(const std::string& modelPath) {
    initStatus_ = loadModel(modelPath);
    return initStatus_.isReady();
}

void FakeDetector::release() {
    unloadModel();
}

InitializationError FakeDetector::loadModel(const std::string& modelPath) {
    if (modelPath.empty()) {
        initStatus_ = {
            InitializationStatus::Ready,
            "",
            "fake_model"
        };
    } else {
        initStatus_ = {
            InitializationStatus::Ready,
            "",
            modelPath
        };
        modelInfo_.modelPath = modelPath;
    }

    isReady_ = true;
    return initStatus_;
}

void FakeDetector::unloadModel() {
    isReady_ = false;
    initStatus_ = {
        InitializationStatus::Uninitialized,
        "",
        ""
    };
}

InitializationError FakeDetector::getInitializationStatus() const {
    return initStatus_;
}

DetectionList FakeDetector::detect(const Frame& frame) {
    // Track call count
    detectCallCount_++;

    // Simulate inference delay if configured
    if (inferenceDelayMs_ > 0.0f) {
        std::this_thread::sleep_for(
            std::chrono::duration<float, std::milli>(inferenceDelayMs_)
        );
    }

    // Return empty list if not initialized
    if (!isReady_) {
        return {};
    }

    // Return predefined results (copy to allow modification)
    DetectionList results = predefinedResults_;

    // Apply confidence filtering if enabled
    if (filterByConfidence_ && config_.confidenceThreshold > 0.0f) {
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [this](const Detection& det) {
                    return det.confidence < config_.confidenceThreshold;
                }),
            results.end()
        );
    }

    return results;
}

void FakeDetector::setConfig(const DetectorConfig& config) {
    config_ = config;
}

const DetectorConfig& FakeDetector::getConfig() const {
    return config_;
}

ModelInfo FakeDetector::getModelInfo() const {
    return modelInfo_;
}

DetectorStats FakeDetector::getPerformanceStats() const {
    // Update total time if inference delay is set
    if (inferenceDelayMs_ > 0.0f) {
        DetectorStats stats = perfStats_;
        stats.inferenceTimeMs = inferenceDelayMs_;
        stats.totalTimeMs = stats.preProcessTimeMs + stats.inferenceTimeMs + stats.postProcessTimeMs;
        return stats;
    }
    return perfStats_;
}

void FakeDetector::loadPredefinedResults(const std::vector<Detection>& results) {
    predefinedResults_ = results;
}

} // namespace test
} // namespace macroman
