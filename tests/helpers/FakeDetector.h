#pragma once

#include "core/interfaces/IDetector.h"
#include "core/entities/Detection.h"
#include "core/entities/Frame.h"
#include <vector>
#include <string>
#include <atomic>

namespace macroman {
namespace test {

/**
 * @brief Fake detector for integration testing
 *
 * Provides pre-configured detection results for testing the pipeline
 * without requiring actual AI inference or GPU resources.
 *
 * Usage:
 *   FakeDetector detector;
 *   detector.loadPredefinedResults({Detection{BBox{100, 100, 50, 80}, 0.9f, 0, HitboxType::Head}});
 *   detector.initialize("");
 *
 *   DetectionList results = detector.detect(frame);  // Returns predefined results
 *
 * Features:
 * - Configurable detection results (return same results every frame)
 * - Detection count tracking (how many times detect() was called)
 * - Configurable delay simulation (mimic inference latency)
 * - Performance stats (fake timing data)
 * - No GPU dependencies (suitable for headless CI/CD)
 *
 * Limitations:
 * - Returns same detections every frame (no actual image analysis)
 * - No model loading/inference (just returns pre-configured data)
 * - Not suitable for testing actual AI detection logic
 */
class FakeDetector : public IDetector {
public:
    FakeDetector() = default;
    ~FakeDetector() override = default;

    // IDetector interface
    bool initialize(const std::string& modelPath) override;
    void release() override;

    InitializationError loadModel(const std::string& modelPath) override;
    void unloadModel() override;
    InitializationError getInitializationStatus() const override;

    DetectionList detect(const Frame& frame) override;

    void setConfig(const DetectorConfig& config) override;
    const DetectorConfig& getConfig() const override;

    std::string getName() const override { return "FakeDetector"; }
    bool isReady() const override { return isReady_; }
    ModelInfo getModelInfo() const override;
    int getNumberOfClasses() const override { return modelInfo_.numClasses; }

    DetectorStats getPerformanceStats() const override;

    // Test utilities

    /**
     * @brief Load predefined detection results (returned every frame)
     * @param results Vector of Detection objects to return
     */
    void loadPredefinedResults(const std::vector<Detection>& results);

    /**
     * @brief Set inference delay (simulate processing time)
     * @param delayMs Delay in milliseconds (0 = instant)
     */
    void setInferenceDelay(float delayMs) { inferenceDelayMs_ = delayMs; }

    /**
     * @brief Get total number of detect() calls
     */
    [[nodiscard]] size_t getDetectCallCount() const { return detectCallCount_; }

    /**
     * @brief Reset detection call counter
     */
    void resetCallCount() { detectCallCount_ = 0; }

    /**
     * @brief Enable/disable confidence filtering
     * @param enabled If true, filter detections by config.confidenceThreshold
     */
    void setConfidenceFilteringEnabled(bool enabled) { filterByConfidence_ = enabled; }

private:
    std::vector<Detection> predefinedResults_;
    size_t detectCallCount_{0};
    float inferenceDelayMs_{0.0f};
    bool isReady_{false};
    bool filterByConfidence_{false};

    ModelInfo modelInfo_{
        640,           // inputWidth
        640,           // inputHeight
        3,             // numClasses (head, chest, body)
        "fake_model",  // modelPath
        "FakeBackend"  // backendName
    };

    InitializationError initStatus_{InitializationStatus::Uninitialized, "", ""};

    DetectorStats perfStats_{
        0.5f,  // preProcessTimeMs
        5.0f,  // inferenceTimeMs
        0.3f,  // postProcessTimeMs
        5.8f   // totalTimeMs
    };
};

} // namespace test
} // namespace macroman
