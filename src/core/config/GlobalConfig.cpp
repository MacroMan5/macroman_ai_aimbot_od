#include "GlobalConfig.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace macroman {

// Helper: Trim whitespace from string
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");

    if (start == std::string::npos) {
        return "";
    }

    return str.substr(start, end - start + 1);
}

// Helper: Convert string to lowercase
static std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

bool GlobalConfig::validate() const {
    if (maxFPS < 30 || maxFPS > 240) {
        return false;  // FPS must be reasonable
    }

    if (vramBudgetMB < 128 || vramBudgetMB > 2048) {
        return false;  // VRAM budget must be reasonable
    }

    std::string logLevelLower = toLower(logLevel);
    if (logLevelLower != "trace" && logLevelLower != "debug" &&
        logLevelLower != "info" && logLevelLower != "warn" &&
        logLevelLower != "error" && logLevelLower != "critical") {
        return false;  // Invalid log level
    }

    if (theme != "Dark" && theme != "Light") {
        return false;  // Invalid theme
    }

    return true;
}

void GlobalConfig::applyDefaults() {
    if (maxFPS <= 0) maxFPS = 144;
    if (vramBudgetMB == 0) vramBudgetMB = 512;
    if (logLevel.empty()) logLevel = "Info";
    if (overlayHotkey.empty()) overlayHotkey = "HOME";
    if (theme.empty()) theme = "Dark";
    if (sharedMemoryName.empty()) sharedMemoryName = "MacromanAimbot_Config";
    if (commandPipeName.empty()) commandPipeName = "MacromanAimbot_Commands";
}

bool GlobalConfigManager::loadFromFile(const std::string& filePath) {
    LOG_INFO("GlobalConfigManager: Loading config from {}", filePath);

    if (!parseINI(filePath)) {
        return false;
    }

    config_.applyDefaults();

    if (!config_.validate()) {
        lastError_ = "Configuration validation failed";
        LOG_ERROR("GlobalConfigManager: {}", lastError_);
        return false;
    }

    LOG_INFO("GlobalConfigManager: Configuration loaded successfully");
    LOG_INFO("  MaxFPS: {}", config_.maxFPS);
    LOG_INFO("  VRAM Budget: {} MB", config_.vramBudgetMB);
    LOG_INFO("  Log Level: {}", config_.logLevel);
    LOG_INFO("  Thread Affinity: {}", config_.threadAffinity ? "enabled" : "disabled");
    LOG_INFO("  Overlay Hotkey: {}", config_.overlayHotkey);
    LOG_INFO("  Show Debug Info: {}", config_.showDebugInfo ? "yes" : "no");
    LOG_INFO("  Theme: {}", config_.theme);
    LOG_INFO("  Shared Memory: {}", config_.sharedMemoryName);

    return true;
}

bool GlobalConfigManager::parseINI(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastError_ = "Failed to open file: " + filePath;
        LOG_ERROR("GlobalConfigManager: {}", lastError_);
        return false;
    }

    std::string currentSection;
    std::map<std::string, std::string> currentKeyValues;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            // Save previous section
            if (!currentSection.empty()) {
                parseSection(currentSection, currentKeyValues);
                currentKeyValues.clear();
            }

            currentSection = line.substr(1, line.length() - 2);
            currentSection = trim(currentSection);
            continue;
        }

        // Parse key=value
        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            LOG_WARN("GlobalConfigManager: Invalid line {} in {}: {}", lineNumber, filePath, line);
            continue;
        }

        std::string key = trim(line.substr(0, equalPos));
        std::string value = trim(line.substr(equalPos + 1));

        currentKeyValues[key] = value;
    }

    // Save last section
    if (!currentSection.empty()) {
        parseSection(currentSection, currentKeyValues);
    }

    return true;
}

void GlobalConfigManager::parseSection(const std::string& section,
                                        const std::map<std::string, std::string>& keyValues) {
    if (section == "Engine") {
        for (const auto& [key, value] : keyValues) {
            if (key == "MaxFPS") {
                config_.maxFPS = std::stoi(value);
            } else if (key == "VRAMBudget") {
                config_.vramBudgetMB = std::stoull(value);
            } else if (key == "LogLevel") {
                config_.logLevel = value;
            } else if (key == "ThreadAffinity") {
                config_.threadAffinity = (toLower(value) == "true" || value == "1");
            }
        }
    } else if (section == "UI") {
        for (const auto& [key, value] : keyValues) {
            if (key == "OverlayHotkey") {
                config_.overlayHotkey = value;
            } else if (key == "ShowDebugInfo") {
                config_.showDebugInfo = (toLower(value) == "true" || value == "1");
            } else if (key == "Theme") {
                config_.theme = value;
            }
        }
    } else if (section == "IPC") {
        for (const auto& [key, value] : keyValues) {
            if (key == "SharedMemoryName") {
                config_.sharedMemoryName = value;
            } else if (key == "CommandPipeName") {
                config_.commandPipeName = value;
            }
        }
    } else {
        LOG_WARN("GlobalConfigManager: Unknown section [{}]", section);
    }
}

} // namespace macroman
