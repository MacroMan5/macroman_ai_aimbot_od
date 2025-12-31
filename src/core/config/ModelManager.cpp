#include "ModelManager.h"
#include "utils/Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace macroman {

ModelManager::ModelManager() = default;

ModelManager::~ModelManager() {
    shutdown();
}

bool ModelManager::initialize() {
    LOG_INFO("ModelManager: Initializing");

    // MVP: No initialization needed (ONNX Runtime setup deferred)
    return true;
}

void ModelManager::shutdown() {
    if (!modelLoaded_) {
        return;
    }

    LOG_INFO("ModelManager: Shutting down");
    unloadCurrentModel();
}

bool ModelManager::loadModel(const std::string& modelPath) {
    // Validate model file
    if (!validateModelFile(modelPath)) {
        return false;
    }

    // Unload previous model (enforce single-model policy)
    if (modelLoaded_) {
        LOG_INFO("ModelManager: Unloading previous model: {}", currentModelPath_);
        unloadCurrentModel();
    }

    // MVP Stub: Simulate model loading
    LOG_INFO("ModelManager: Loading model: {}", modelPath);

    // In full implementation, this would:
    // 1. Pause detection thread
    // 2. Create ONNX session
    // 3. Allocate GPU buffers
    // 4. Resume detection thread

    // For now, just track state
    currentModelPath_ = modelPath;
    modelLoaded_.store(true, std::memory_order_release);

    // Simulate VRAM usage (typical YOLO model: 200-400MB)
    vramUsageMB_.store(300, std::memory_order_release);

    LOG_INFO("ModelManager: Model loaded successfully ({} MB VRAM)", vramUsageMB_.load());

    // Fire callback if registered
    if (switchCallback_) {
        switchCallback_(modelPath, true, "");
    }

    return true;
}

bool ModelManager::switchModel(const std::string& modelPath) {
    LOG_INFO("ModelManager: Switching to model: {}", modelPath);

    // Attempt to load new model
    // If it fails, keep the old one (no downtime)
    std::string previousModel = currentModelPath_;
    bool previousLoaded = modelLoaded_.load(std::memory_order_acquire);

    if (!loadModel(modelPath)) {
        // Restore previous state on failure
        currentModelPath_ = previousModel;
        modelLoaded_.store(previousLoaded, std::memory_order_release);

        LOG_ERROR("ModelManager: Failed to switch model, keeping previous: {}", previousModel);

        // Fire callback with error
        if (switchCallback_) {
            switchCallback_(modelPath, false, lastError_);
        }

        return false;
    }

    LOG_INFO("ModelManager: Model switched successfully");
    return true;
}

bool ModelManager::reloadCurrentModel() {
    if (!modelLoaded_) {
        lastError_ = "No model loaded to reload";
        LOG_ERROR("ModelManager: {}", lastError_);
        return false;
    }

    LOG_INFO("ModelManager: Reloading current model: {}", currentModelPath_);

    std::string modelPath = currentModelPath_;
    return switchModel(modelPath);
}

void ModelManager::setModelSwitchCallback(ModelSwitchCallback callback) {
    switchCallback_ = std::move(callback);
}

std::string ModelManager::getCurrentModelPath() const {
    return currentModelPath_;
}

bool ModelManager::hasModelLoaded() const {
    return modelLoaded_.load(std::memory_order_acquire);
}

size_t ModelManager::getVRAMUsage() const {
    return vramUsageMB_.load(std::memory_order_acquire);
}

const std::string& ModelManager::getLastError() const {
    return lastError_;
}

bool ModelManager::validateModelFile(const std::string& path) {
    // Check file exists
    if (!fs::exists(path)) {
        lastError_ = "Model file not found: " + path;
        LOG_ERROR("ModelManager: {}", lastError_);
        return false;
    }

    // Check file extension
    fs::path filepath(path);
    if (filepath.extension() != ".onnx") {
        lastError_ = "Invalid model format (expected .onnx): " + path;
        LOG_ERROR("ModelManager: {}", lastError_);
        return false;
    }

    // Check file size (YOLO models should be > 1MB, < 1GB)
    auto fileSize = fs::file_size(filepath);
    if (fileSize < 1024 * 1024) {
        lastError_ = "Model file too small (< 1MB): " + path;
        LOG_ERROR("ModelManager: {}", lastError_);
        return false;
    }

    if (fileSize > 1024 * 1024 * 1024) {
        lastError_ = "Model file too large (> 1GB): " + path;
        LOG_ERROR("ModelManager: {}", lastError_);
        return false;
    }

    return true;
}

void ModelManager::unloadCurrentModel() {
    if (!modelLoaded_) {
        return;
    }

    LOG_INFO("ModelManager: Unloading model: {}", currentModelPath_);

    // MVP Stub: Simulate unload
    // In full implementation, this would:
    // 1. Pause detection thread
    // 2. Release ONNX session
    // 3. Free GPU buffers
    // 4. Resume detection thread

    currentModelPath_.clear();
    modelLoaded_.store(false, std::memory_order_release);
    vramUsageMB_.store(0, std::memory_order_release);

    LOG_INFO("ModelManager: Model unloaded");
}

} // namespace macroman
