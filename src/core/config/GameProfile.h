#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace macroman {

/**
 * @brief Hitbox mapping from model class ID to type
 *
 * Example: {"0": "head", "1": "chest", "2": "body"}
 */
using HitboxMapping = std::map<int, std::string>;

/**
 * @brief Detection configuration for game profile
 */
struct DetectionConfig {
    std::string modelPath;                  // Path to .onnx model (relative to project root)
    std::pair<int, int> inputSize{640, 640}; // Model input dimensions [width, height]
    float confidenceThreshold{0.6f};        // Min confidence for detections [0.0, 1.0]
    float nmsThreshold{0.45f};              // IoU threshold for NMS [0.0, 1.0]
    HitboxMapping hitboxMapping;            // Class ID → hitbox type mapping
};

/**
 * @brief Targeting configuration for game profile
 */
struct TargetingConfig {
    float fov{80.0f};                       // Field of view radius (pixels from crosshair)
    float smoothness{0.65f};                // Aim smoothness [0.0=instant, 1.0=very smooth]
    float predictionStrength{0.8f};         // Kalman prediction weight [0.0, 1.0]
    std::vector<std::string> hitboxPriority{"head", "chest", "body"}; // Priority order
    float inputLatency{12.5f};              // Estimated input lag (ms) for this game
};

/**
 * @brief Complete game profile
 *
 * Loaded from JSON file in `config/games/{gameId}.json`
 */
struct GameProfile {
    std::string gameId;                     // Unique identifier (e.g., "valorant")
    std::string displayName;                // Human-readable name (e.g., "Valorant")
    std::vector<std::string> processNames;  // Process name patterns (e.g., ["VALORANT.exe"])
    std::vector<std::string> windowTitles;  // Window title patterns (e.g., ["VALORANT"])

    DetectionConfig detection;
    TargetingConfig targeting;

    /**
     * @brief Check if process/window matches this profile
     * @param processName Current process name (e.g., "VALORANT.exe")
     * @param windowTitle Current window title (e.g., "VALORANT - Main Menu")
     */
    [[nodiscard]] bool matches(const std::string& processName,
                               const std::string& windowTitle = "") const;
};

inline bool GameProfile::matches(const std::string& processName,
                                  const std::string& windowTitle) const {
    // Check process name
    for (const auto& pattern : processNames) {
        if (processName.find(pattern) != std::string::npos) {
            return true;
        }
    }

    // Check window title (if provided)
    if (!windowTitle.empty()) {
        for (const auto& pattern : windowTitles) {
            if (windowTitle.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

} // namespace macroman
