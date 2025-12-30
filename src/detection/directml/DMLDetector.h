#pragma once

#include "../../core/interfaces/IDetector.h"
#include <onnxruntime_cxx_api.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <mutex>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace sunone {

class DMLDetector : public IDetector {
public:
    DMLDetector();
    ~DMLDetector() override;

    // IDetector interface
    bool initialize(const std::string& modelPath) override;
    void release() override;
    DetectionList detect(const Frame& frame) override;

    std::string getName() const override { return "DirectML"; }
    bool isReady() const override;
    ModelInfo getModelInfo() const override;
    int getNumberOfClasses() const override;

    // Two-phase init
    InitializationError loadModel(const std::string& modelPath) override;
    void unloadModel() override;
    InitializationError getInitializationStatus() const override;
    DetectorStats getPerformanceStats() const override { return stats_; }

private:
    Ort::Env env_{nullptr};
    std::unique_ptr<Ort::Session> session_;
    Ort::SessionOptions sessionOptions_{nullptr};
    
    // Model info
    std::vector<const char*> inputNames_;
    std::vector<const char*> outputNames_;
    std::vector<Ort::AllocatedStringPtr> inputNameAllocatedStrings_; // Keep strings alive
    std::vector<Ort::AllocatedStringPtr> outputNameAllocatedStrings_;
    
    ModelInfo modelInfo_;
    bool ready_ = false;
    InitializationError initStatus_{InitializationStatus::Uninitialized, "", ""};

    // Performance tracking
    mutable DetectorStats stats_;

    // ---------------------------------------------------------
    // GPU Preprocessing Resources (Hybrid Pipeline)
    // ---------------------------------------------------------
    struct GpuResources {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ComPtr<ID3D11ComputeShader> computeShader;
        ComPtr<ID3D11Buffer> constantBuffer;
        ComPtr<ID3D11Buffer> outputBuffer;      // GPU side
        ComPtr<ID3D11Buffer> stagingBuffer;     // CPU readback
        ComPtr<ID3D11UnorderedAccessView> uav;
        ComPtr<ID3D11SamplerState> sampler;
        
        bool initialized = false;
    } gpu_;

    struct AsyncBuffer {
        std::vector<float> inputValues; // We still need this for CPU-side ORT Session::Run
        // Future optimization: D3D12 direct binding
        ComPtr<ID3D11Buffer> gpuPreprocessedBuffer;
        ComPtr<ID3D11Buffer> stagingBuffer;
        bool inFlight = false;
        Frame metadata;
    };
    std::vector<AsyncBuffer> bufferPool_;
    int currentWriteIdx_ = 0;
    int currentReadIdx_ = 0;
    const int poolSize_ = 3;

    struct PreprocessConstants {
        uint32_t inputWidth;
        uint32_t inputHeight;
        uint32_t outputWidth;
        uint32_t outputHeight;
        float roiLeft;
        float roiTop;
        float roiWidth;
        float roiHeight;
    };

    // Helpers
    bool initGpuResources(ID3D11Device* device);
    void releaseGpuResources();
    bool compileComputeShader();
};

} // namespace sunone
