#include "DetectorFactory.h"
#include <stdexcept>

#ifdef USE_CUDA
#include "tensorrt/TensorRTDetector.h"
#include <cuda_runtime.h>
#else
#include "directml/DMLDetector.h"
#endif

namespace sunone {

std::unique_ptr<IDetector> DetectorFactory::create(DetectorType type) {
    switch (type) {
        case DetectorType::TensorRT:
#ifdef USE_CUDA
            if (isAvailable(DetectorType::TensorRT)) {
                return std::make_unique<TensorRTDetector>();
            }
            throw std::runtime_error("TensorRT not available: No CUDA device found");
#else
            throw std::runtime_error("TensorRT not available: CUDA not enabled in build");
#endif

        case DetectorType::DirectML:
#ifndef USE_CUDA
            return std::make_unique<DMLDetector>();
#else
            throw std::runtime_error("DirectML not available in CUDA build");
#endif

        default:
            throw std::runtime_error("Unknown detector type");
    }
}

DetectorType DetectorFactory::getBestAvailable() {
#ifdef USE_CUDA
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err == cudaSuccess && deviceCount > 0) {
        return DetectorType::TensorRT;
    }
    // In CUDA builds, DirectML is not available (excluded from build)
    // So we cannot fall back to it.
    throw std::runtime_error("No CUDA-capable device found. DirectML fallback is not available in CUDA builds.");
#else
    // DirectML is always available on Windows in DML builds
    return DetectorType::DirectML;
#endif
}

bool DetectorFactory::isAvailable(DetectorType type) {
    switch (type) {
        case DetectorType::TensorRT:
#ifdef USE_CUDA
            {
                int deviceCount = 0;
                cudaError_t err = cudaGetDeviceCount(&deviceCount);
                return err == cudaSuccess && deviceCount > 0;
            }
#else
            return false;
#endif

        case DetectorType::DirectML:
#ifndef USE_CUDA
            // DirectML is available in DML builds
            return true;
#else
            // DirectML not available in CUDA builds
            return false;
#endif

        default:
            return false;
    }
}

std::vector<DetectorType> DetectorFactory::getAvailableBackends() {
    std::vector<DetectorType> backends;

    // Check in order of preference
    if (isAvailable(DetectorType::TensorRT)) {
        backends.push_back(DetectorType::TensorRT);
    }
    if (isAvailable(DetectorType::DirectML)) {
        backends.push_back(DetectorType::DirectML);
    }

    return backends;
}

const char* DetectorFactory::getTypeName(DetectorType type) {
    switch (type) {
        case DetectorType::TensorRT:
            return "TensorRT";
        case DetectorType::DirectML:
            return "DirectML";
        default:
            return "Unknown";
    }
}

} // namespace sunone
