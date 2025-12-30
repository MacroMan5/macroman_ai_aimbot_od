#ifdef USE_CUDA

#include "TensorRTDetector.h"
#include "../postprocess/PostProcessor.h"
#include "../../core/utils/PathUtils.h"
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#include <cuda_d3d11_interop.h>
#include <d3d11.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

#pragma comment(lib, "cudart.lib")

// External kernel declaration
extern "C" void launchPreprocessKernel(
    cudaTextureObject_t texObj,
    float* outputDeviceBuffer,
    int inputWidth, int inputHeight,
    int outputWidth, int outputHeight,
    int roiX, int roiY, int roiW, int roiH,
    cudaStream_t stream
);

namespace sunone {

// TensorRT logger
class TRTLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;

TensorRTDetector::TensorRTDetector()
    : ready_(false)
{
}

TensorRTDetector::~TensorRTDetector() {
    release();
}

bool TensorRTDetector::initialize(const std::string& modelPath) {
    // Delegate to loadModel for backward compatibility
    auto result = loadModel(modelPath);
    return result.isReady();
}

InitializationError TensorRTDetector::loadModel(const std::string& modelPath) {
    initStatus_.status = InitializationStatus::Initializing;
    initStatus_.attemptedPath = modelPath;

    try {
        // Normalize path
        std::string normalizedPath = PathUtils::normalize(modelPath);
        std::cout << "[TensorRTDetector] Loading model: " << normalizedPath << std::endl;

        // Check file exists
        std::ifstream file(normalizedPath);
        if (!file.good()) {
            initStatus_.status = InitializationStatus::Failed;
            initStatus_.errorMessage = "Model file not found";
            std::cerr << "[TensorRTDetector] File not found: " << normalizedPath << std::endl;
            std::cerr << "[TensorRTDetector] Working directory: "
                      << std::filesystem::current_path() << std::endl;
            ready_ = false;
            return initStatus_;
        }
        file.close();
        std::cout << "[TensorRTDetector] Model file exists" << std::endl;

        // Note: Using internal config for logging verbosity if available
        // const auto config = ConfigManager::instance().getConfig();
        // Here we can use config_.verboseLogging if set.

        // Check if it's an engine file or ONNX file
        bool isEngine = normalizedPath.find(".engine") != std::string::npos;
        std::cout << "[TensorRTDetector] Detected file type: " << (isEngine ? "TensorRT Engine" : "ONNX") << std::endl;

        if (isEngine) {
            std::cout << "[TensorRTDetector] Loading TensorRT engine..." << std::endl;
            if (!loadEngine(normalizedPath)) {
                initStatus_.status = InitializationStatus::Failed;
                initStatus_.errorMessage = "Failed to load TensorRT engine";
                ready_ = false;
                return initStatus_;
            }
        } else {
            std::cout << "[TensorRTDetector] Building TensorRT engine from ONNX..." << std::endl;
            if (!buildEngineFromONNX(normalizedPath)) {
                initStatus_.status = InitializationStatus::Failed;
                initStatus_.errorMessage = "Failed to build engine from ONNX";
                ready_ = false;
                return initStatus_;
            }
        }
        std::cout << "[TensorRTDetector] Engine loaded successfully" << std::endl;

        std::cout << "[TensorRTDetector] Getting tensor information..." << std::endl;
        const char* inputName = engine_->getIOTensorName(0);
        const char* outputName = engine_->getIOTensorName(1);
        std::cout << "[TensorRTDetector] Input tensor: " << inputName << ", Output tensor: " << outputName << std::endl;

        auto inputDims = engine_->getTensorShape(inputName);
        auto outputDims = engine_->getTensorShape(outputName);

        modelInfo_.modelPath = normalizedPath;
        modelInfo_.backendName = "TensorRT";
        modelInfo_.inputWidth = static_cast<int>(inputDims.d[3]);
        modelInfo_.inputHeight = static_cast<int>(inputDims.d[2]);
        modelInfo_.numClasses = static_cast<int>(outputDims.d[1]) - 4;

        std::cout << "[TensorRTDetector] Input shape: [" << inputDims.d[0] << ", " << inputDims.d[1]
                  << ", " << inputDims.d[2] << ", " << inputDims.d[3] << "]" << std::endl;
        std::cout << "[TensorRTDetector] Output shape: [" << outputDims.d[0] << ", " << outputDims.d[1]
                  << ", " << outputDims.d[2] << "]" << std::endl;

        // Allocate CUDA buffers
        std::cout << "[TensorRTDetector] Allocating CUDA buffers..." << std::endl;
        size_t inputSize = 1 * 3 * modelInfo_.inputHeight * modelInfo_.inputWidth * sizeof(float);
        size_t outputSize = outputDims.d[0] * outputDims.d[1] * outputDims.d[2] * sizeof(float);

        cudaError_t cudaErr = cudaMalloc(&inputDeviceBuffer_, inputSize);
        if (cudaErr != cudaSuccess) {
            initStatus_.status = InitializationStatus::Failed;
            initStatus_.errorMessage = "Failed to allocate input CUDA buffer: " + std::string(cudaGetErrorString(cudaErr));
            ready_ = false;
            return initStatus_;
        }

        cudaErr = cudaMalloc(&outputDeviceBuffer_, outputSize);
        if (cudaErr != cudaSuccess) {
            initStatus_.status = InitializationStatus::Failed;
            initStatus_.errorMessage = "Failed to allocate output CUDA buffer: " + std::string(cudaGetErrorString(cudaErr));
            cudaFree(inputDeviceBuffer_);
            inputDeviceBuffer_ = nullptr;
            ready_ = false;
            return initStatus_;
        }
        std::cout << "[TensorRTDetector] CUDA buffers allocated (Input: " << inputSize << " bytes, Output: " << outputSize << " bytes)" << std::endl;

        std::cout << "[TensorRTDetector] Creating CUDA stream..." << std::endl;
        cudaError_t streamErr = cudaStreamCreate(&cudaStream_);
        if (streamErr != cudaSuccess) {
            initStatus_.status = InitializationStatus::Failed;
            initStatus_.errorMessage = "Failed to create CUDA stream: " + std::string(cudaGetErrorString(streamErr));
            cudaFree(inputDeviceBuffer_);
            cudaFree(outputDeviceBuffer_);
            inputDeviceBuffer_ = nullptr;
            outputDeviceBuffer_ = nullptr;
            ready_ = false;
            return initStatus_;
        }
        std::cout << "[TensorRTDetector] CUDA stream created" << std::endl;

        ready_ = true;
        initStatus_.status = InitializationStatus::Ready;
        initStatus_.errorMessage = "";

        if (config_.verboseLogging) {
            std::cout << "[TensorRTDetector] Initialized successfully" << std::endl;
            std::cout << "  Model: " << normalizedPath << std::endl;
            std::cout << "  Input: [1, 3, " << modelInfo_.inputHeight << ", " << modelInfo_.inputWidth << "]" << std::endl;
            std::cout << "  Classes: " << modelInfo_.numClasses << std::endl;
        }

        std::cout << "[TensorRTDetector] Initialization complete!" << std::endl;
        return initStatus_;

    } catch (const std::exception& e) {
        ready_ = false;
        initStatus_.status = InitializationStatus::Failed;
        initStatus_.errorMessage = "Error: " + std::string(e.what());
        std::cerr << "[TensorRTDetector] Exception: " << e.what() << std::endl;
        return initStatus_;
    } catch (...) {
        ready_ = false;
        initStatus_.status = InitializationStatus::Failed;
        initStatus_.errorMessage = "Unknown error during model loading";
        std::cerr << "[TensorRTDetector] Unknown exception" << std::endl;
        return initStatus_;
    }
}

void TensorRTDetector::unloadModel() {
    release();
    ready_ = false;
    initStatus_ = {InitializationStatus::Uninitialized, "", ""};
    std::cout << "[TensorRTDetector] Model unloaded" << std::endl;
}

InitializationError TensorRTDetector::getInitializationStatus() const {
    return initStatus_;
}

bool TensorRTDetector::loadEngine(const std::string& enginePath) {
    std::cout << "[TensorRTDetector::loadEngine] Reading engine file: " << enginePath << std::endl;

    std::ifstream file(enginePath, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[TensorRTDetector::loadEngine] Engine file not found or cannot be opened" << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::cout << "[TensorRTDetector::loadEngine] Engine file size: " << size << " bytes" << std::endl;

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    file.close();
    std::cout << "[TensorRTDetector::loadEngine] Engine file read into memory" << std::endl;

    std::cout << "[TensorRTDetector::loadEngine] Creating TensorRT runtime..." << std::endl;
    runtime_ = nvinfer1::createInferRuntime(gLogger);
    if (!runtime_) {
        std::cerr << "[TensorRTDetector::loadEngine] Failed to create TensorRT runtime" << std::endl;
        return false;
    }

    std::cout << "[TensorRTDetector::loadEngine] Deserializing CUDA engine..." << std::endl;
    engine_ = runtime_->deserializeCudaEngine(buffer.data(), size);
    if (!engine_) {
        std::cerr << "[TensorRTDetector::loadEngine] Failed to deserialize CUDA engine" << std::endl;
        std::cerr << "[TensorRTDetector::loadEngine] This may indicate:" << std::endl;
        std::cerr << "  - Engine was built with different TensorRT version" << std::endl;
        std::cerr << "  - Engine was built for different GPU architecture" << std::endl;
        std::cerr << "  - Engine file is corrupted" << std::endl;
        return false;
    }
    std::cout << "[TensorRTDetector::loadEngine] Engine deserialized successfully" << std::endl;

    std::cout << "[TensorRTDetector::loadEngine] Creating execution context..." << std::endl;
    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "[TensorRTDetector::loadEngine] Failed to create execution context" << std::endl;
        return false;
    }
    std::cout << "[TensorRTDetector::loadEngine] Execution context created" << std::endl;

    return true;
}

bool TensorRTDetector::buildEngineFromONNX(const std::string& onnxPath) {
    std::cout << "[TensorRTDetector::buildEngineFromONNX] Building from ONNX: " << onnxPath << std::endl;

    std::ifstream file(onnxPath, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] ONNX file not found or cannot be opened" << std::endl;
        return false;
    }
    file.close();
    std::cout << "[TensorRTDetector::buildEngineFromONNX] ONNX file accessible" << std::endl;

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating TensorRT runtime..." << std::endl;
    runtime_ = nvinfer1::createInferRuntime(gLogger);
    if (!runtime_) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to create runtime" << std::endl;
        return false;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating builder..." << std::endl;
    auto builder = nvinfer1::createInferBuilder(gLogger);
    if (!builder) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to create builder" << std::endl;
        return false;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating network..." << std::endl;
    auto network = builder->createNetworkV2(1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH));
    if (!network) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to create network" << std::endl;
        delete builder;
        return false;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating ONNX parser..." << std::endl;
    auto parser = nvonnxparser::createParser(*network, gLogger);
    if (!parser) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to create ONNX parser" << std::endl;
        delete network;
        delete builder;
        return false;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Parsing ONNX file (this may take a while)..." << std::endl;
    if (!parser->parseFromFile(onnxPath.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to parse ONNX file" << std::endl;
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Parser errors:" << std::endl;
        for (int i = 0; i < parser->getNbErrors(); ++i) {
            std::cerr << "  " << parser->getError(i)->desc() << std::endl;
        }
        delete parser;
        delete network;
        delete builder;
        return false;
    }
    std::cout << "[TensorRTDetector::buildEngineFromONNX] ONNX file parsed successfully" << std::endl;

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating builder config..." << std::endl;
    auto config = builder->createBuilderConfig();
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 1 << 30); // 1GB
    std::cout << "[TensorRTDetector::buildEngineFromONNX] Workspace size: 1GB" << std::endl;

    // Enable FP16 if supported
    if (builder->platformHasFastFp16()) {
        std::cout << "[TensorRTDetector::buildEngineFromONNX] FP16 supported, enabling..." << std::endl;
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
    } else {
        std::cout << "[TensorRTDetector::buildEngineFromONNX] FP16 not supported, using FP32" << std::endl;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Building TensorRT engine (this will take several minutes)..." << std::endl;
    auto serializedEngine = builder->buildSerializedNetwork(*network, *config);
    if (!serializedEngine) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to build engine" << std::endl;
        delete config;
        delete parser;
        delete network;
        delete builder;
        return false;
    }
    std::cout << "[TensorRTDetector::buildEngineFromONNX] Engine built successfully!" << std::endl;

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Deserializing engine..." << std::endl;
    engine_ = runtime_->deserializeCudaEngine(serializedEngine->data(), serializedEngine->size());
    if (!engine_) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to deserialize engine" << std::endl;
        delete serializedEngine;
        delete config;
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    std::cout << "[TensorRTDetector::buildEngineFromONNX] Creating execution context..." << std::endl;
    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Failed to create execution context" << std::endl;
        delete serializedEngine;
        delete config;
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    // Save engine for future use
    std::string enginePath = onnxPath.substr(0, onnxPath.find_last_of('.')) + ".engine";
    std::cout << "[TensorRTDetector::buildEngineFromONNX] Saving engine to: " << enginePath << std::endl;
    std::ofstream engineFile(enginePath, std::ios::binary);
    if (engineFile.good()) {
        engineFile.write(static_cast<const char*>(serializedEngine->data()), serializedEngine->size());
        std::cout << "[TensorRTDetector::buildEngineFromONNX] Engine saved successfully (" << serializedEngine->size() << " bytes)" << std::endl;
    } else {
        std::cerr << "[TensorRTDetector::buildEngineFromONNX] Warning: Failed to save engine file" << std::endl;
    }

    delete serializedEngine;
    delete config;
    delete parser;
    delete network;
    delete builder;

    return true;
}

void TensorRTDetector::release() {
    if (ready_) {
        ready_ = false;
        // Wait for pending operations
        cudaStreamSynchronize(cudaStream_);
    }
    
    unregisterTexture();

    if (inputDeviceBuffer_) {
        cudaFree(inputDeviceBuffer_);
        inputDeviceBuffer_ = nullptr;
    }
    if (outputDeviceBuffer_) {
        cudaFree(outputDeviceBuffer_);
        outputDeviceBuffer_ = nullptr;
    }
    if (cudaStream_) {
        cudaStreamDestroy(cudaStream_);
        cudaStream_ = nullptr;
    }
    if (context_) {
        delete context_;
        context_ = nullptr;
    }
    if (engine_) {
        delete engine_;
        engine_ = nullptr;
    }
    if (runtime_) {
        delete runtime_;
        runtime_ = nullptr;
    }
} 

bool TensorRTDetector::registerTexture(ID3D11Texture2D* texture) {
    if (registeredTexture_ == texture) return true; // Already registered

    unregisterTexture();

    std::cout << "[TensorRTDetector] Registering D3D11 texture with CUDA..." << std::endl;
    cudaError_t err = cudaGraphicsD3D11RegisterResource(&cudaResource_, texture, cudaGraphicsRegisterFlagsNone);
    if (err != cudaSuccess) {
        std::cerr << "[TensorRTDetector] cudaGraphicsD3D11RegisterResource failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    registeredTexture_ = texture;
    return true;
}

void TensorRTDetector::unregisterTexture() {
    if (cudaTexture_) {
        cudaDestroyTextureObject(cudaTexture_);
        cudaTexture_ = 0;
    }
    if (cudaResource_) {
        cudaGraphicsUnregisterResource(cudaResource_);
        cudaResource_ = nullptr;
    }
    registeredTexture_ = nullptr;
}

DetectionList TensorRTDetector::detect(const Frame& frame) {
    if (!ready_ || frame.empty()) {
        return {};
    }

    auto startTotal = std::chrono::high_resolution_clock::now();
    stats_.totalTimeMs = 0.0f;
    stats_.preProcessTimeMs = 0.0f;
    stats_.inferenceTimeMs = 0.0f;
    stats_.postProcessTimeMs = 0.0f;

    try {
        const auto& config = config_;

        // ---------------------------------------------------------
        // PRE-PROCESSING
        // ---------------------------------------------------------
        auto startPre = std::chrono::high_resolution_clock::now();

        // Check for GPU texture (Zero-Copy Path)
        if (frame.gpuTexture && config.useGpuAcceleration) {
            if (!registerTexture(frame.gpuTexture.Get())) {
                // Fallback to CPU if registration fails
                std::cerr << "[TensorRTDetector] GPU registration failed, falling back to CPU" << std::endl;
                goto cpu_fallback;
            }

            // Map resource
            cudaError_t err = cudaGraphicsMapResources(1, &cudaResource_, cudaStream_);
            if (err != cudaSuccess) {
                 std::cerr << "[TensorRTDetector] Map resources failed" << std::endl;
                 goto cpu_fallback;
            }

            cudaArray_t cuArray;
            err = cudaGraphicsSubResourceGetMappedArray(&cuArray, cudaResource_, 0, 0);
            if (err != cudaSuccess) {
                cudaGraphicsUnmapResources(1, &cudaResource_, cudaStream_);
                goto cpu_fallback;
            }

            // Create texture object if needed (or recreate if array changed)
            // Ideally we cache this unless texture changes
            if (cudaTexture_ == 0) {
                cudaResourceDesc resDesc;
                memset(&resDesc, 0, sizeof(resDesc));
                resDesc.resType = cudaResourceTypeArray;
                resDesc.res.array.array = cuArray;

                cudaTextureDesc texDesc;
                memset(&texDesc, 0, sizeof(texDesc));
                texDesc.addressMode[0] = cudaAddressModeClamp;
                texDesc.addressMode[1] = cudaAddressModeClamp;
                texDesc.filterMode = cudaFilterModeLinear;
                texDesc.readMode = cudaReadModeNormalizedFloat; // Important: Get 0.0-1.0 float
                texDesc.normalizedCoords = 0; // Use non-normalized coords (0..width)

                err = cudaCreateTextureObject(&cudaTexture_, &resDesc, &texDesc, nullptr);
                if (err != cudaSuccess) {
                     cudaGraphicsUnmapResources(1, &cudaResource_, cudaStream_);
                     goto cpu_fallback;
                }
            }

            // Run Kernel
            launchPreprocessKernel(
                cudaTexture_,
                static_cast<float*>(inputDeviceBuffer_),
                frame.width, frame.height, // These are usually ROI size if cropped, or full size
                modelInfo_.inputWidth, modelInfo_.inputHeight,
                frame.roiX, frame.roiY, frame.width, frame.height,
                cudaStream_
            );
            
            cudaGraphicsUnmapResources(1, &cudaResource_, cudaStream_);

        } else {
        cpu_fallback:
            // Legacy CPU Path
            // Preprocess image
            cv::Mat resized;
            if (frame.image.empty()) return {}; // Should not happen if gpuTexture was null
            
            cv::resize(frame.image, resized, cv::Size(modelInfo_.inputWidth, modelInfo_.inputHeight));
            cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
            resized.convertTo(resized, CV_32FC3, 1.0f / 255.0f);

            // Convert to tensor format [1, 3, H, W]
            std::vector<cv::Mat> channels(3);
            cv::split(resized, channels);

            const size_t planeSize = static_cast<size_t>(modelInfo_.inputHeight) * modelInfo_.inputWidth;
            std::vector<float> inputTensorValues(1 * 3 * planeSize);

            for (int c = 0; c < 3; ++c) {
                memcpy(&inputTensorValues[c * planeSize], channels[c].data, planeSize * sizeof(float));
            }

            // Copy input to device
            size_t inputSize = inputTensorValues.size() * sizeof(float);
            cudaMemcpyAsync(inputDeviceBuffer_, inputTensorValues.data(), inputSize, cudaMemcpyHostToDevice, cudaStream_);
        }

        auto endPre = std::chrono::high_resolution_clock::now();
        stats_.preProcessTimeMs = std::chrono::duration<float, std::milli>(endPre - startPre).count();

        // ---------------------------------------------------------
        // INFERENCE
        // ---------------------------------------------------------
        auto startInf = std::chrono::high_resolution_clock::now();

        // Set tensor addresses (TensorRT 10+ API)
        const char* inputName = engine_->getIOTensorName(0);
        const char* outputName = engine_->getIOTensorName(1);

        context_->setTensorAddress(inputName, inputDeviceBuffer_);
        context_->setTensorAddress(outputName, outputDeviceBuffer_);

        // Run inference asynchronously
        if (!context_->enqueueV3(cudaStream_)) {
            std::cerr << "[TensorRTDetector] Inference failed" << std::endl;
            return {};
        }

        auto endInf = std::chrono::high_resolution_clock::now();
        stats_.inferenceTimeMs = std::chrono::duration<float, std::milli>(endInf - startInf).count();

        // ---------------------------------------------------------
        // POST-PROCESSING
        // ---------------------------------------------------------
        auto startPost = std::chrono::high_resolution_clock::now();

        // Get output dimensions using tensor name
        auto outputDims = engine_->getTensorShape(outputName);
        int rows = static_cast<int>(outputDims.d[1]);
        int cols = static_cast<int>(outputDims.d[2]);
        size_t outputSize = outputDims.d[0] * rows * cols * sizeof(float);

        std::vector<float> outputData(outputDims.d[0] * rows * cols);
        // Wait for inference to finish and copy back
        cudaMemcpyAsync(outputData.data(), outputDeviceBuffer_, outputSize, cudaMemcpyDeviceToHost, cudaStream_);
        cudaStreamSynchronize(cudaStream_);

        // Post-process detections
        DetectionList detections;
        float confThreshold = config.confidenceThreshold;
        float nmsThreshold = config.nmsThreshold;

        // Parse YOLO output (YOLOv11 format: [x, y, w, h, class_scores...])
        for (int i = 0; i < cols; ++i) {
            const float* detection = outputData.data() + i;

            // Find max class score
            float maxClassScore = 0.0f;
            int maxClassId = 0;
            for (int c = 0; c < modelInfo_.numClasses; ++c) {
                float classScore = detection[(4 + c) * cols];
                if (classScore > maxClassScore) {
                    maxClassScore = classScore;
                    maxClassId = c;
                }
            }

            if (maxClassScore < confThreshold) {
                continue;
            }

            // Extract box coordinates
            float cx = detection[0 * cols];
            float cy = detection[1 * cols];
            float w = detection[2 * cols];
            float h = detection[3 * cols];

            // Convert from center format to corner format
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

        // Apply NMS
        DetectionList finalDetections = PostProcessor::applyNMS(detections, nmsThreshold);

        auto endPost = std::chrono::high_resolution_clock::now();
        stats_.postProcessTimeMs = std::chrono::duration<float, std::milli>(endPost - startPost).count();
        stats_.totalTimeMs = std::chrono::duration<float, std::milli>(endPost - startTotal).count();

        return finalDetections;

    } catch (const std::exception& e) {
        std::cerr << "[TensorRTDetector] Detection error: " << e.what() << std::endl;
        return {};
    }
}

bool TensorRTDetector::isReady() const {
    return ready_;
}

ModelInfo TensorRTDetector::getModelInfo() const {
    return modelInfo_;
}

} // namespace sunone

#endif // USE_CUDA
