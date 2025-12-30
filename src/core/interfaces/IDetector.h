#pragma once

#include "../entities/Frame.h"
#include "../entities/Detection.h"
#include <vector>
#include <string>

namespace macroman {

struct ModelInfo {
    int inputWidth;
    int inputHeight;
    int numClasses;
    std::string modelPath;
    std::string backendName;
};

struct DetectorConfig {
    float confidenceThreshold = 0.5f;
    float nmsThreshold = 0.4f;
    float detectionResolution = 1.0f; // Scale factor if detection runs at different res
    bool useGpuAcceleration = true;
    bool verboseLogging = false;
};

/**
 * Initialization status for detectors
 */
enum class InitializationStatus {
    Uninitialized,  // No model loaded
    Initializing,   // Loading in progress
    Ready,          // Model loaded successfully
    Failed          // Model loading failed
};

/**
 * Detailed initialization error information
 */
struct InitializationError {
    InitializationStatus status = InitializationStatus::Uninitialized;
    std::string errorMessage;
    std::string attemptedPath;

    bool isReady() const { return status == InitializationStatus::Ready; }
    bool hasFailed() const { return status == InitializationStatus::Failed; }
};

struct DetectorStats {
    float preProcessTimeMs = 0.0f;
    float inferenceTimeMs = 0.0f;
    float postProcessTimeMs = 0.0f;
    float totalTimeMs = 0.0f;
};

class IDetector {
public:
    virtual ~IDetector() = default;

    // Lifecycle
    virtual bool initialize(const std::string& modelPath) = 0;
    virtual void release() = 0;

    // Two-phase initialization for graceful error handling
    virtual InitializationError loadModel(const std::string& modelPath) = 0;
    virtual void unloadModel() = 0;
    virtual InitializationError getInitializationStatus() const = 0;

    // NEW: Async operation for pipeline overlap
    virtual void enqueueDetect(const Frame& frame) {
        // Default implementation just runs synchronously
        detect(frame); 
    }
    virtual DetectionList getLatestResults() { return {}; }

    // Core operation
    virtual DetectionList detect(const Frame& frame) = 0;

    // Configuration
    virtual void setConfig(const DetectorConfig& config) { config_ = config; }
    virtual const DetectorConfig& getConfig() const { return config_; }

    // Info
    virtual std::string getName() const = 0;
    virtual bool isReady() const = 0;
    virtual ModelInfo getModelInfo() const = 0;
    virtual int getNumberOfClasses() const = 0;
    
    // Performance
    virtual DetectorStats getPerformanceStats() const = 0;

protected:
    DetectorConfig config_;
};

} // namespace macroman