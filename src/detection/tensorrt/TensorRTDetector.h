#pragma once

#ifdef USE_CUDA

#include "../../core/interfaces/IDetector.h"
#include <NvInfer.h>
#include <cuda_runtime.h>
#include <memory>
#include <string>
#include <vector>

// Forward decl
struct ID3D11Texture2D;
struct cudaGraphicsResource;

namespace sunone {

/**
 * TensorRT-based object detector for NVIDIA GPUs.
 * Only available when USE_CUDA is defined.
 */
class TensorRTDetector : public IDetector {
public:
    TensorRTDetector();
    ~TensorRTDetector() override;

    // IDetector interface
    bool initialize(const std::string& modelPath) override;
    void release() override;
    DetectionList detect(const Frame& frame) override;

    std::string getName() const override { return "TensorRT"; }
    bool isReady() const override;
    ModelInfo getModelInfo() const override;
    int getNumberOfClasses() const override { return modelInfo_.numClasses; }
    DetectorStats getPerformanceStats() const override { return stats_; }

    // NEW: Two-phase initialization
    InitializationError loadModel(const std::string& modelPath) override;
    void unloadModel() override;
    InitializationError getInitializationStatus() const override;

private:
    // TensorRT objects
    nvinfer1::IRuntime* runtime_ = nullptr;
    nvinfer1::ICudaEngine* engine_ = nullptr;
    nvinfer1::IExecutionContext* context_ = nullptr;

    // CUDA buffers
    void* inputDeviceBuffer_ = nullptr;
    void* outputDeviceBuffer_ = nullptr;

    // CUDA stream for async execution
    cudaStream_t cudaStream_ = nullptr;

    // Triple Buffering for Pipeline Overlap
    struct AsyncBuffer {
        void* inputDevice = nullptr;
        void* outputDevice = nullptr;
        cudaEvent_t event = nullptr;
        bool inFlight = false;
        Frame metadata; // Store timestamp/id for the detections
    };
    std::vector<AsyncBuffer> bufferPool_;
    int currentWriteIdx_ = 0;
    int currentReadIdx_ = 0;
    const int poolSize_ = 3; // Triple buffering

    // CUDA Interop Resources
    cudaGraphicsResource* cudaResource_ = nullptr;
    cudaTextureObject_t cudaTexture_ = 0;
    ID3D11Texture2D* registeredTexture_ = nullptr; // Track which texture is registered

    // Model information
    ModelInfo modelInfo_;
    bool ready_ = false;
    
    // Performance stats
    mutable DetectorStats stats_;

    // Initialization status
    InitializationError initStatus_{InitializationStatus::Uninitialized, "", ""};

    // Helper methods
    bool loadEngine(const std::string& enginePath);
    bool buildEngineFromONNX(const std::string& onnxPath);
    
    // Interop helpers
    bool registerTexture(ID3D11Texture2D* texture);
    void unregisterTexture();
};

} // namespace sunone

#endif // USE_CUDA
