#pragma once

#include "../entities/Frame.h"
#include "../entities/Detection.h"
#include <vector>
#include <string>

namespace macroman {

// Type alias for detection results
using DetectionList = std::vector<Detection>;

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

/**
 * @brief Object detection abstraction
 *
 * Implementations:
 * - DMLDetector: DirectML backend (8-12ms inference, YOLO-based)
 * - TensorRTDetector: NVIDIA TensorRT backend (5-8ms inference)
 *
 * @note Thread Safety: All methods must be called from the same thread (Detection Thread).
 * The Detection thread owns this object and calls detect() at ~100-144 FPS.
 *
 * Lifecycle:
 * 1. loadModel() - Load ONNX model (blocking, ~2-5 seconds)
 * 2. detect() - Repeatedly process frames
 * 3. unloadModel() - Clean up GPU resources
 */
class IDetector {
public:
    virtual ~IDetector() = default;

    /**
     * @brief Initialize detector with model file
     * @param modelPath Path to ONNX model file (e.g., "assets/models/yolov8n.onnx")
     * @return true if initialization succeeded
     * @deprecated Use loadModel() for better error handling
     */
    virtual bool initialize(const std::string& modelPath) = 0;

    /**
     * @brief Release GPU resources (blocking)
     * @deprecated Use unloadModel() for consistency
     */
    virtual void release() = 0;

    /**
     * @brief Load ONNX model with detailed error reporting
     * @param modelPath Absolute or relative path to .onnx file
     * @return InitializationError with status and error details
     *
     * @note Thread Safety: Must be called before any detect() calls
     * @note Performance: Blocking operation, typically 2-5 seconds
     */
    virtual InitializationError loadModel(const std::string& modelPath) = 0;

    /**
     * @brief Unload model and release GPU memory
     *
     * @note Thread Safety: Must not be called while detect() is running
     */
    virtual void unloadModel() = 0;

    /**
     * @brief Get current initialization state
     * @return Status and error message (if any)
     */
    virtual InitializationError getInitializationStatus() const = 0;

    /**
     * @brief Enqueue frame for async detection (pipeline overlap)
     * @param frame Input frame from Capture thread
     *
     * @note Default implementation runs synchronously via detect()
     * @note Thread Safety: Called by Detection thread only
     */
    virtual void enqueueDetect(const Frame& frame) {
        // Default implementation just runs synchronously
        detect(frame);
    }

    /**
     * @brief Get results from async detection
     * @return Detection list from most recent enqueued frame
     *
     * @note Thread Safety: Called by Detection thread only
     */
    virtual DetectionList getLatestResults() { return {}; }

    /**
     * @brief Synchronously detect objects in frame
     * @param frame Input frame with GPU texture
     * @return Vector of detections (bounding boxes + class IDs)
     *
     * @note Performance Target: 5-12ms (DirectML), 5-8ms (TensorRT)
     * @note Thread Safety: Called by Detection thread only
     */
    virtual DetectionList detect(const Frame& frame) = 0;

    /**
     * @brief Update detection configuration at runtime
     * @param config New confidence/NMS thresholds
     *
     * @note Thread Safety: Safe to call from any thread (atomic update)
     */
    virtual void setConfig(const DetectorConfig& config) { config_ = config; }

    /**
     * @brief Get current detector configuration
     * @return Copy of current config
     */
    virtual const DetectorConfig& getConfig() const { return config_; }

    /**
     * @brief Get detector implementation name
     * @return "DirectML YOLO", "TensorRT YOLO", etc.
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Check if detector is ready for inference
     * @return true if model is loaded and GPU context is valid
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Get loaded model metadata
     * @return Model input dimensions, class count, etc.
     */
    virtual ModelInfo getModelInfo() const = 0;

    /**
     * @brief Get number of detection classes
     * @return Class count from ONNX model metadata
     */
    virtual int getNumberOfClasses() const = 0;

    /**
     * @brief Get last detection performance metrics
     * @return Pre/post-process and inference timings
     */
    virtual DetectorStats getPerformanceStats() const = 0;

protected:
    DetectorConfig config_;
};

} // namespace macroman