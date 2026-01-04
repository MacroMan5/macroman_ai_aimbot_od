#pragma once

#include <string>
#include <map>

namespace macroman {

/**
 * @brief Global configuration (Level 1 of 3-tier hierarchy)
 *
 * Architecture Reference: Section "Configuration Hierarchy (3 Levels)"
 *
 * Hierarchy:
 * 1. Global Config (config/global.ini) - This class
 * 2. Game Profiles (config/games/*.json) - ProfileManager
 * 3. Runtime Config (Shared Memory) - SharedConfig
 *
 * Purpose: Application-wide settings loaded once at startup
 *
 * Thread Safety: NOT thread-safe. Load once from main thread before
 * starting pipeline threads.
 */
struct GlobalConfig {
    // [Engine]
    int maxFPS{144};                    // Target capture FPS
    size_t vramBudgetMB{512};           // Max VRAM for aimbot (MB)
    std::string logLevel{"Info"};       // Trace/Debug/Info/Warn/Error/Critical
    bool threadAffinity{true};          // Pin threads to specific cores

    // [UI]
    std::string overlayHotkey{"HOME"};  // Key to toggle debug overlay
    bool showDebugInfo{true};           // Show metrics/bboxes by default
    std::string theme{"Dark"};          // UI theme (Dark/Light)

    // [IPC]
    std::string sharedMemoryName{"MacromanAimbot_Config"};
    std::string commandPipeName{"MacromanAimbot_Commands"};

    /**
     * @brief Validate configuration values
     * @return true if all values are valid
     */
    [[nodiscard]] bool validate() const;

    /**
     * @brief Apply defaults for missing values
     */
    void applyDefaults();
};

/**
 * @brief Global configuration manager
 *
 * Responsibilities:
 * - Load global.ini file
 * - Parse INI format (simple key=value parser)
 * - Validate ranges and required fields
 * - Provide singleton-style access
 *
 * Thread Safety: Load once from main thread, read-only access from others
 */
class GlobalConfigManager {
public:
    GlobalConfigManager() = default;
    ~GlobalConfigManager() = default;

    // Non-copyable, movable
    GlobalConfigManager(const GlobalConfigManager&) = delete;
    GlobalConfigManager& operator=(const GlobalConfigManager&) = delete;
    GlobalConfigManager(GlobalConfigManager&&) = default;
    GlobalConfigManager& operator=(GlobalConfigManager&&) = default;

    /**
     * @brief Load configuration from INI file
     * @param filePath Path to global.ini
     * @return true if loaded successfully
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const GlobalConfig& getConfig() const { return config_; }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const { return lastError_; }

private:
    GlobalConfig config_;
    std::string lastError_;

    // INI parsing helpers
    bool parseINI(const std::string& filePath);
    void parseSection(const std::string& section, const std::map<std::string, std::string>& keyValues);
};

} // namespace macroman
