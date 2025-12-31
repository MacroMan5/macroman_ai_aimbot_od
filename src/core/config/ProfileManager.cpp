#include "ProfileManager.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace macroman {

bool ProfileManager::loadProfile(const std::string& filePath) {
    try {
        // Check file exists
        if (!fs::exists(filePath)) {
            lastError_ = "File not found: " + filePath;
            LOG_ERROR("ProfileManager: {}", lastError_);
            return false;
        }

        // Parse JSON
        std::ifstream file(filePath);
        json j = json::parse(file);

        // Extract profile fields
        GameProfile profile;
        profile.gameId = j.at("gameId").get<std::string>();
        profile.displayName = j.at("displayName").get<std::string>();
        profile.processNames = j.at("processNames").get<std::vector<std::string>>();
        profile.windowTitles = j.at("windowTitles").get<std::vector<std::string>>();

        // Detection config
        auto& det = j.at("detection");
        profile.detection.modelPath = det.at("modelPath").get<std::string>();

        auto inputSize = det.at("inputSize").get<std::vector<int>>();
        profile.detection.inputSize = {inputSize[0], inputSize[1]};

        profile.detection.confidenceThreshold = det.at("confidenceThreshold").get<float>();
        profile.detection.nmsThreshold = det.at("nmsThreshold").get<float>();

        // Hitbox mapping (keys are strings in JSON, convert to int)
        auto hitboxMap = det.at("hitboxMapping").get<std::map<std::string, std::string>>();
        for (const auto& [key, value] : hitboxMap) {
            profile.detection.hitboxMapping[std::stoi(key)] = value;
        }

        // Targeting config
        auto& tgt = j.at("targeting");
        profile.targeting.fov = tgt.at("fov").get<float>();
        profile.targeting.smoothness = tgt.at("smoothness").get<float>();
        profile.targeting.predictionStrength = tgt.at("predictionStrength").get<float>();
        profile.targeting.hitboxPriority = tgt.at("hitboxPriority").get<std::vector<std::string>>();
        profile.targeting.inputLatency = tgt.at("inputLatency").get<float>();

        // Validate and apply defaults
        applyDefaults(profile);
        if (!validateProfile(profile)) {
            return false;
        }

        // Add to registry
        profiles_.push_back(profile);
        LOG_INFO("ProfileManager: Loaded profile '{}' from {}", profile.gameId, filePath);
        return true;

    } catch (const json::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        LOG_ERROR("ProfileManager: Failed to load {}: {}", filePath, lastError_);
        return false;
    } catch (const std::exception& e) {
        lastError_ = std::string("Error loading profile: ") + e.what();
        LOG_ERROR("ProfileManager: {}", lastError_);
        return false;
    }
}

const GameProfile* ProfileManager::getProfile(const std::string& gameId) const {
    for (const auto& profile : profiles_) {
        if (profile.gameId == gameId) {
            return &profile;
        }
    }
    return nullptr;
}

const GameProfile* ProfileManager::findProfileByProcess(
    const std::string& processName,
    const std::string& windowTitle
) const {
    for (const auto& profile : profiles_) {
        if (profile.matches(processName, windowTitle)) {
            return &profile;
        }
    }
    return nullptr;
}

bool ProfileManager::validateProfile(const GameProfile& profile) {
    // Required fields
    if (profile.gameId.empty()) {
        lastError_ = "gameId is required";
        LOG_ERROR("ProfileManager: {}", lastError_);
        return false;
    }

    if (profile.processNames.empty() && profile.windowTitles.empty()) {
        lastError_ = "At least one processName or windowTitle required";
        LOG_ERROR("ProfileManager: {}", lastError_);
        return false;
    }

    // Validate ranges
    if (profile.detection.confidenceThreshold < 0.0f ||
        profile.detection.confidenceThreshold > 1.0f) {
        lastError_ = "confidenceThreshold must be in [0.0, 1.0]";
        LOG_ERROR("ProfileManager: {}", lastError_);
        return false;
    }

    if (profile.targeting.smoothness < 0.0f || profile.targeting.smoothness > 1.0f) {
        lastError_ = "smoothness must be in [0.0, 1.0]";
        LOG_ERROR("ProfileManager: {}", lastError_);
        return false;
    }

    return true;
}

void ProfileManager::applyDefaults(GameProfile& profile) {
    // Apply defaults for optional fields
    if (profile.displayName.empty()) {
        profile.displayName = profile.gameId;
    }

    if (profile.detection.inputSize.first == 0) {
        profile.detection.inputSize = {640, 640};
    }

    if (profile.targeting.hitboxPriority.empty()) {
        profile.targeting.hitboxPriority = {"head", "chest", "body"};
    }
}

size_t ProfileManager::loadProfilesFromDirectory(const std::string& directory) {
    size_t loadedCount = 0;

    if (!fs::exists(directory)) {
        lastError_ = "Directory not found: " + directory;
        LOG_ERROR("ProfileManager: {}", lastError_);
        return 0;
    }

    LOG_INFO("ProfileManager: Loading profiles from {}", directory);

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") {
            if (loadProfile(entry.path().string())) {
                ++loadedCount;
            }
        }
    }

    LOG_INFO("ProfileManager: Loaded {} profiles", loadedCount);
    return loadedCount;
}

} // namespace macroman
