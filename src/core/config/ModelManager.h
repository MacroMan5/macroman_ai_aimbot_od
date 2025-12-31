#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace macroman {

/**
 * @brief Callback when model switch completes
 * @param modelPath Path to newly loaded model
 * @param success True if switch succeeded
 * @param errorMessage Error description if failed
 */
using ModelSwitchCallback = std::function<void(const std::string& modelPath, bool success, const std::string& errorMessage)>;

/**
 * @brief Thread-safe AI model manager (MVP: single model)
 *
 * Responsibilities:
 * - Load/unload ONNX models on demand
 * - Thread-safe model switching (pause detection thread, swap, resume)
 * - VRAM budget enforcement (<512MB)
 * - Error handling with fallback to previous model
 *
 * Architecture Note: MVP uses single model only (no preloading).
 * Multi-model support deferred to post-MVP.
 *
 * Thread Safety: Thread-safe. Can be called from any thread.
 * Model switching pauses Detection thread during swap.
 *
 * Lifecycle:
 * 1. initialize() - Setup resources
 * 2. loadModel(path) - Load initial model
 * 3. switchModel(path) - Hot-swap models
 * 4. shutdown() - Cleanup
 */
class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    // Non-copyable, movable
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager(ModelManager&&) = default;
    ModelManager& operator=(ModelManager&&) = default;

    /**
     * @brief Initialize model manager
     * @return true if initialized successfully
     */
    bool initialize();

    /**
     * @brief Shutdown and release resources
     */
    void shutdown();

    /**
     * @brief Load model (initial load or switch)
     * @param modelPath Path to .onnx model file
     * @return true if loaded successfully
     *
     * Thread Safety: Pauses detection thread during load
     * VRAM Enforcement: Unloads previous model before loading new
     */
    bool loadModel(const std::string& modelPath);

    /**
     * @brief Switch to different model (hot-swap)
     * @param modelPath Path to .onnx model file
     * @return true if switch succeeded
     *
     * Behavior on Failure:
     * - Keeps previous model loaded (no downtime)
     * - Logs error via getLastError()
     */
    bool switchModel(const std::string& modelPath);

    /**
     * @brief Reload current model from disk
     * @return true if reloaded successfully
     *
     * Use Case: Model file updated on disk (e.g., after training)
     */
    bool reloadCurrentModel();

    /**
     * @brief Register callback for model switch events
     */
    void setModelSwitchCallback(ModelSwitchCallback callback);

    /**
     * @brief Get path to currently loaded model
     * @return Model path, or empty string if no model loaded
     */
    [[nodiscard]] std::string getCurrentModelPath() const;

    /**
     * @brief Check if a model is currently loaded
     */
    [[nodiscard]] bool hasModelLoaded() const;

    /**
     * @brief Get estimated VRAM usage (MB)
     */
    [[nodiscard]] size_t getVRAMUsage() const;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const;

private:
    // Current model state
    std::string currentModelPath_;
    std::atomic<bool> modelLoaded_{false};
    std::atomic<size_t> vramUsageMB_{0};

    // Error tracking
    std::string lastError_;

    // Callback for switch events
    ModelSwitchCallback switchCallback_;

    // Internal helpers
    bool validateModelFile(const std::string& path);
    void unloadCurrentModel();
};

} // namespace macroman
