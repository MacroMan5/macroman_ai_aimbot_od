#pragma once

#include "GameProfile.h"
#include <memory>
#include <vector>
#include <optional>
#include <string>

namespace macroman {

/**
 * @brief Game profile manager
 *
 * Responsibilities:
 * - Load profiles from JSON files in config/games/
 * - Validate profiles (required fields, valid ranges)
 * - Provide fallback defaults for missing/invalid values
 * - Track active profile
 *
 * Thread Safety: NOT thread-safe. Call from main thread only.
 *
 * Lifecycle:
 * 1. loadProfilesFromDirectory("config/games")
 * 2. getProfile(gameId) or findProfileByProcess(processName)
 * 3. setActiveProfile(gameId)
 */
class ProfileManager {
public:
    ProfileManager() = default;
    ~ProfileManager() = default;

    // Non-copyable, movable
    ProfileManager(const ProfileManager&) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;
    ProfileManager(ProfileManager&&) = default;
    ProfileManager& operator=(ProfileManager&&) = default;

    /**
     * @brief Load all profiles from directory
     * @param directory Path to directory containing .json files
     * @return Number of profiles loaded successfully
     *
     * Logs errors for invalid profiles but continues loading others.
     */
    size_t loadProfilesFromDirectory(const std::string& directory);

    /**
     * @brief Load single profile from file
     * @param filePath Path to .json file
     * @return true if loaded successfully
     */
    bool loadProfile(const std::string& filePath);

    /**
     * @brief Get profile by game ID
     * @param gameId Unique game identifier (e.g., "valorant")
     * @return Pointer to profile, or nullptr if not found
     */
    [[nodiscard]] const GameProfile* getProfile(const std::string& gameId) const;

    /**
     * @brief Find profile matching process/window
     * @param processName Current process name
     * @param windowTitle Current window title (optional)
     * @return Pointer to matching profile, or nullptr
     */
    [[nodiscard]] const GameProfile* findProfileByProcess(
        const std::string& processName,
        const std::string& windowTitle = ""
    ) const;

    /**
     * @brief Get all loaded profiles
     */
    [[nodiscard]] const std::vector<GameProfile>& getAllProfiles() const {
        return profiles_;
    }

    /**
     * @brief Get count of loaded profiles
     */
    [[nodiscard]] size_t getProfileCount() const { return profiles_.size(); }

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& getLastError() const { return lastError_; }

private:
    std::vector<GameProfile> profiles_;
    std::string lastError_;

    // Validation helpers
    bool validateProfile(const GameProfile& profile);
    void applyDefaults(GameProfile& profile);
};

} // namespace macroman
