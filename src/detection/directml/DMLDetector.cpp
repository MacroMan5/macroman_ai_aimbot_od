#include "DMLDetector.h"
#include "../postprocess/PostProcessor.h"
#include "../../core/utils/PathUtils.h"
#include <dml_provider_factory.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <d3dcompiler.h>

namespace sunone {

DMLDetector::DMLDetector()
    : env_(ORT_LOGGING_LEVEL_WARNING, "SunoneAimbot_DML"),
      ready_(false)
{
}

DMLDetector::~DMLDetector() {
    release();
}

bool DMLDetector::initialize(const std::string& modelPath) {
    // Delegate to loadModel for backward compatibility
    auto result = loadModel(modelPath);
    return result.isReady();
}

InitializationError DMLDetector::loadModel(const std::string& modelPath) {
    initStatus_.status = InitializationStatus::Initializing;
    initStatus_.attemptedPath = modelPath;

    try {
        std::string normalizedPath = PathUtils::normalize(modelPath);
        std::cout << "[DMLDetector] Loading model: " << normalizedPath << std::endl;

        if (!std::filesystem::exists(normalizedPath)) {
            initStatus_.status = InitializationStatus::Failed;
            initStatus_.errorMessage = "Model file not found";
            ready_ = false;
            return initStatus_;
        }

        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        // DirectML Best Practices:
        // Disable memory pattern optimization as DML handles memory management
        sessionOptions.DisableMemPattern();
        // Set sequential execution as DML is single-threaded submission (and fast)
        sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        
        try {
            OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0);
            std::cout << "[DMLDetector] DirectML provider enabled" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DMLDetector] DirectML error: " << e.what() << std::endl;
        }

        std::wstring wPath(normalizedPath.begin(), normalizedPath.end());
        session_ = std::make_unique<Ort::Session>(env_, wPath.c_str(), sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;

        size_t numInputs = session_->GetInputCount();
        inputNames_.clear();
        inputNameAllocatedStrings_.clear();
        for (size_t i = 0; i < numInputs; i++) {
            auto name = session_->GetInputNameAllocated(i, allocator);
            inputNames_.push_back(name.get());
            inputNameAllocatedStrings_.push_back(std::move(name));
        }

        size_t numOutputs = session_->GetOutputCount();
        outputNames_.clear();
        outputNameAllocatedStrings_.clear();
        for (size_t i = 0; i < numOutputs; i++) {
            auto name = session_->GetOutputNameAllocated(i, allocator);
            outputNames_.push_back(name.get());
            outputNameAllocatedStrings_.push_back(std::move(name));
        }

        auto inputTypeInfo = session_->GetInputTypeInfo(0);
        auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
        auto inputDims = inputTensorInfo.GetShape();
        
        modelInfo_.inputWidth = static_cast<int>(inputDims[3]);
        modelInfo_.inputHeight = static_cast<int>(inputDims[2]);
        modelInfo_.modelPath = normalizedPath;
        modelInfo_.backendName = "DirectML";

        auto outputTypeInfo = session_->GetOutputTypeInfo(0);
        auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
        auto outputDims = outputTensorInfo.GetShape();
        modelInfo_.numClasses = static_cast<int>(outputDims[1]) - 4;

        ready_ = true;
        initStatus_.status = InitializationStatus::Ready;
        return initStatus_;

    } catch (const std::exception& e) {
        ready_ = false;
        initStatus_.status = InitializationStatus::Failed;
        initStatus_.errorMessage = e.what();
        return initStatus_;
    }
}

void DMLDetector::unloadModel() {
    release();
    ready_ = false;
    initStatus_ = {InitializationStatus::Uninitialized, "", ""};
    std::cout << "[DMLDetector] Model unloaded" << std::endl;
}

InitializationError DMLDetector::getInitializationStatus() const {
    return initStatus_;
}

void DMLDetector::release() {
    releaseGpuResources();
    session_.reset();
    ready_ = false;
}

// ---------------------------------------------------------
// GPU Resource Management
// ---------------------------------------------------------

bool DMLDetector::compileComputeShader() {
    // Load shader source
    std::vector<std::string> searchPaths = {
        "InputPreprocessing.hlsl",
        "extracted_modules/detection/directml/InputPreprocessing.hlsl", // New structure
        "src/detection/directml/InputPreprocessing.hlsl" // Fallback
    };
    
    std::string shaderPath;
    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            shaderPath = path;
            break;
        }
    }

    if (shaderPath.empty()) {
        std::cerr << "[DMLDetector] Compute shader not found" << std::endl;
        return false;
    }

    std::ifstream shaderFile(shaderPath);
    if (!shaderFile.good()) {
        std::cerr << "[DMLDetector] Compute shader not readable: " << shaderPath << std::endl;
        return false;
    }

    std::string shaderSource((std::istreambuf_iterator<char>(shaderFile)), 
                              std::istreambuf_iterator<char>());

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errorBlob;
    
    HRESULT hr = D3DCompile(
        shaderSource.c_str(), shaderSource.length(),
        nullptr, nullptr, nullptr,
        "main", "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        blob.GetAddressOf(), errorBlob.GetAddressOf()
    );

    if (FAILED(hr)) {
        std::cerr << "[DMLDetector] Shader compilation failed: 0x" << std::hex << hr << std::endl;
        if (errorBlob) {
            std::cerr << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = gpu_.device->CreateComputeShader(
        blob->GetBufferPointer(), blob->GetBufferSize(),
        nullptr, gpu_.computeShader.GetAddressOf()
    );

    return SUCCEEDED(hr);
}

bool DMLDetector::initGpuResources(ID3D11Device* device) {
    if (gpu_.initialized && gpu_.device.Get() == device) return true;

    releaseGpuResources();

    gpu_.device = device;
    device->GetImmediateContext(gpu_.context.GetAddressOf());

    if (!compileComputeShader()) {
        std::cerr << "[DMLDetector] Failed to compile GPU shader" << std::endl;
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(PreprocessConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    if (FAILED(gpu_.device->CreateBuffer(&cbDesc, nullptr, gpu_.constantBuffer.GetAddressOf())))
        return false;

    uint32_t elementCount = 3 * modelInfo_.inputWidth * modelInfo_.inputHeight;
    uint32_t size = elementCount * sizeof(float);

    D3D11_BUFFER_DESC bufDesc = {};
    bufDesc.ByteWidth = size;
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufDesc.StructureByteStride = sizeof(float);
    
    if (FAILED(gpu_.device->CreateBuffer(&bufDesc, nullptr, gpu_.outputBuffer.GetAddressOf())))
        return false;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = elementCount;
    
    if (FAILED(gpu_.device->CreateUnorderedAccessView(gpu_.outputBuffer.Get(), &uavDesc, gpu_.uav.GetAddressOf())))
        return false;

    D3D11_BUFFER_DESC stgDesc = {};
    stgDesc.ByteWidth = size;
    stgDesc.Usage = D3D11_USAGE_STAGING;
    stgDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    if (FAILED(gpu_.device->CreateBuffer(&stgDesc, nullptr, gpu_.stagingBuffer.GetAddressOf())))
        return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    
    if (FAILED(gpu_.device->CreateSamplerState(&sampDesc, gpu_.sampler.GetAddressOf())))
        return false;

    gpu_.initialized = true;
    return true;
}

void DMLDetector::releaseGpuResources() {
    gpu_.sampler.Reset();
    gpu_.stagingBuffer.Reset();
    gpu_.uav.Reset();
    gpu_.outputBuffer.Reset();
    gpu_.constantBuffer.Reset();
    gpu_.computeShader.Reset();
    gpu_.context.Reset();
    gpu_.device.Reset();
    gpu_.initialized = false;
}

DetectionList DMLDetector::detect(const Frame& frame) {
    if (!ready_ || frame.empty()) return {};

    auto startTotal = std::chrono::high_resolution_clock::now();
    stats_.totalTimeMs = 0.0f;
    stats_.preProcessTimeMs = 0.0f;
    stats_.inferenceTimeMs = 0.0f;
    stats_.postProcessTimeMs = 0.0f;

    // Use internal config
    const auto& config = config_;
    
    std::vector<float> inputTensorValues;
    size_t inputTensorSize = 1 * 3 * modelInfo_.inputHeight * modelInfo_.inputWidth;

    try {
        auto startPre = std::chrono::high_resolution_clock::now();

        bool gpuPathUsed = false;
        if (config.useGpuAcceleration && frame.gpuTexture) {
            // HYBRID GPU PATH
            ComPtr<ID3D11Device> textureDevice;
            frame.gpuTexture->GetDevice(textureDevice.GetAddressOf());
            
            if (initGpuResources(textureDevice.Get())) {
                D3D11_MAPPED_SUBRESOURCE map;
                if (SUCCEEDED(gpu_.context->Map(gpu_.constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
                    PreprocessConstants* cb = (PreprocessConstants*)map.pData;
                    
                    D3D11_TEXTURE2D_DESC desc;
                    frame.gpuTexture->GetDesc(&desc);
                    
                    cb->inputWidth = desc.Width;
                    cb->inputHeight = desc.Height;
                    cb->outputWidth = modelInfo_.inputWidth;
                    cb->outputHeight = modelInfo_.inputHeight;
                    
                    // Calculate ROI
                    // Frame.width/height is the logical ROI size
                    // Frame.roiX/Y needs to be added to Frame struct
                    float fullW = static_cast<float>(desc.Width);
                    float fullH = static_cast<float>(desc.Height);
                    
                    cb->roiLeft = static_cast<float>(frame.roiX) / fullW;
                    cb->roiTop = static_cast<float>(frame.roiY) / fullH;
                    cb->roiWidth = static_cast<float>(frame.width) / fullW;
                    cb->roiHeight = static_cast<float>(frame.height) / fullH;

                    gpu_.context->Unmap(gpu_.constantBuffer.Get(), 0);
                }

                ComPtr<ID3D11ShaderResourceView> srv;
                if (SUCCEEDED(gpu_.device->CreateShaderResourceView(frame.gpuTexture.Get(), nullptr, srv.GetAddressOf()))) {
                    gpu_.context->CSSetShader(gpu_.computeShader.Get(), nullptr, 0);
                    ID3D11ShaderResourceView* srvs[] = { srv.Get() };
                    gpu_.context->CSSetShaderResources(0, 1, srvs);
                    ID3D11UnorderedAccessView* uavs[] = { gpu_.uav.Get() };
                    gpu_.context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
                    ID3D11SamplerState* samplers[] = { gpu_.sampler.Get() };
                    gpu_.context->CSSetSamplers(0, 1, samplers);
                    ID3D11Buffer* cbs[] = { gpu_.constantBuffer.Get() };
                    gpu_.context->CSSetConstantBuffers(0, 1, cbs);

                    uint32_t gx = (modelInfo_.inputWidth + 15) / 16;
                    uint32_t gy = (modelInfo_.inputHeight + 15) / 16;
                    gpu_.context->Dispatch(gx, gy, 1);

                    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
                    gpu_.context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

                    gpu_.context->CopyResource(gpu_.stagingBuffer.Get(), gpu_.outputBuffer.Get());

                    if (SUCCEEDED(gpu_.context->Map(gpu_.stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &map))) {
                        inputTensorValues.resize(inputTensorSize);
                        memcpy(inputTensorValues.data(), map.pData, inputTensorSize * sizeof(float));
                        gpu_.context->Unmap(gpu_.stagingBuffer.Get(), 0);
                        gpuPathUsed = true;
                    }
                }
            }
        }

        if (!gpuPathUsed) {
            cv::Mat resized;
            cv::resize(frame.image, resized, cv::Size(modelInfo_.inputWidth, modelInfo_.inputHeight));
            cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
            resized.convertTo(resized, CV_32FC3, 1.0f / 255.0f);

            std::vector<cv::Mat> channels(3);
            cv::split(resized, channels);
            
            inputTensorValues.resize(inputTensorSize);
            size_t planeSize = static_cast<size_t>(modelInfo_.inputHeight) * modelInfo_.inputWidth;
            for (int c = 0; c < 3; ++c) {
                memcpy(&inputTensorValues[c * planeSize], channels[c].data, planeSize * sizeof(float));
            }
        }

        auto endPre = std::chrono::high_resolution_clock::now();
        stats_.preProcessTimeMs = std::chrono::duration<float, std::milli>(endPre - startPre).count();

        auto startInf = std::chrono::high_resolution_clock::now();
        std::vector<int64_t> inputDims = {1, 3, modelInfo_.inputHeight, modelInfo_.inputWidth};
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, inputTensorValues.data(), inputTensorValues.size(), 
            inputDims.data(), inputDims.size()
        );

        auto outputTensors = session_->Run(
            Ort::RunOptions{nullptr}, 
            inputNames_.data(), &inputTensor, 1, 
            outputNames_.data(), 1
        );

        auto endInf = std::chrono::high_resolution_clock::now();
        stats_.inferenceTimeMs = std::chrono::duration<float, std::milli>(endInf - startInf).count();

        auto startPost = std::chrono::high_resolution_clock::now();
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto typeInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        auto outputShape = typeInfo.GetShape();
        int rows = static_cast<int>(outputShape[1]);
        int cols = static_cast<int>(outputShape[2]);

        DetectionList detections;
        float confThreshold = config.confidenceThreshold;
        float nmsThreshold = config.nmsThreshold;

        for (int i = 0; i < cols; ++i) {
            float maxClassScore = 0.0f;
            int maxClassId = 0;
            for (int c = 0; c < modelInfo_.numClasses; ++c) {
                float score = outputData[(4 + c) * cols + i];
                if (score > maxClassScore) {
                    maxClassScore = score;
                    maxClassId = c;
                }
            }

            if (maxClassScore < confThreshold) continue;

            float cx = outputData[0 * cols + i];
            float cy = outputData[1 * cols + i];
            float w = outputData[2 * cols + i];
            float h = outputData[3 * cols + i];

            // Use configured detection resolution
            int x = static_cast<int>((cx - w / 2.0f) * config.detectionResolution);
            int y = static_cast<int>((cy - h / 2.0f) * config.detectionResolution);
            int width = static_cast<int>(w * config.detectionResolution);
            int height = static_cast<int>(h * config.detectionResolution);

            Detection d;
            d.box = cv::Rect(x, y, width, height);
            d.confidence = maxClassScore;
            d.classId = maxClassId;
            detections.push_back(d);
        }

        auto result = PostProcessor::applyNMS(detections, nmsThreshold);
        auto endPost = std::chrono::high_resolution_clock::now();
        
        stats_.postProcessTimeMs = std::chrono::duration<float, std::milli>(endPost - startPost).count();
        stats_.totalTimeMs = std::chrono::duration<float, std::milli>(endPost - startTotal).count();

        return result;

    } catch (const std::exception& e) {
        std::cerr << "[DMLDetector] Detection error: " << e.what() << std::endl;
        return {};
    }
}

bool DMLDetector::isReady() const {
    return ready_;
}

ModelInfo DMLDetector::getModelInfo() const {
    return modelInfo_;
}

int DMLDetector::getNumberOfClasses() const {
    return modelInfo_.numClasses;
}

} // namespace sunone
