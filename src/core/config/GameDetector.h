#pragma once

#include "GameProfile.h"
#include <string>
#include <optional>
#include <chrono>
#include <functional>

namespace macroman {

// Forward declaration
class ProfileManager;

/**
 * @brief Active game information
 */
struct GameInfo {
    std::string processName;    // e.g., "VALORANT.exe"
    std::string windowTitle;    // e.g., "VALORANT - Main Menu"
    void* windowHandle;         // HWND (cast to void* for cross-platform header)
};

/**
 * @brief Callback when game changes
 * @param newProfile Profile for detected game
 * @param gameInfo Window/process information
 */
using GameChangedCallback = std::function<void(const GameProfile& newProfile, const GameInfo& gameInfo)>;

/**
 * @brief Auto game detection with hysteresis
 *
 * Polls foreground window every 500ms. Requires 3-second stable detection
 * before committing game switch (prevents thrashing on alt-tab).
 *
 * Architecture Reference: Section "Auto-Game Detection" with hysteresis
 *
 * Thread Safety: NOT thread-safe. Call poll() from dedicated background thread.
 *
 * Lifecycle:
 * 1. setProfileManager(manager) - Required before polling
 * 2. setGameChangedCallback(callback) - Register notification
 * 3. poll() - Call every 500ms
 * 4. Game detected → callback fired after 3s stability
 */
class GameDetector {
public:
    GameDetector();
    ~GameDetector() = default;

    // Non-copyable, movable
    GameDetector(const GameDetector&) = delete;
    GameDetector& operator=(const GameDetector&) = delete;
    GameDetector(GameDetector&&) = default;
    GameDetector& operator=(GameDetector&&) = default;

    /**
     * @brief Set profile manager (required)
     */
    void setProfileManager(const ProfileManager* manager);

    /**
     * @brief Register game changed callback
     */
    void setGameChangedCallback(GameChangedCallback callback);

    /**
     * @brief Poll foreground window and check for game change
     *
     * Call this every 500ms from background thread.
     *
     * Hysteresis Logic:
     * - New game detected → Start 3-second timer
     * - Same game for 3 seconds → Fire callback
     * - Different game detected → Reset timer
     */
    void poll();

    /**
     * @brief Get current candidate game (may not be committed yet)
     */
    [[nodiscard]] std::optional<std::string> getCandidateGameId() const;

    /**
     * @brief Get time remaining before candidate becomes active (ms)
     */
    [[nodiscard]] int64_t getHysteresisTimeRemaining() const;

    /**
     * @brief Check if currently in hysteresis period
     */
    [[nodiscard]] bool isInHysteresis() const;

private:
    const ProfileManager* profileManager_{nullptr};
    GameChangedCallback gameChangedCallback_;

    // Hysteresis state
    std::optional<std::string> candidateGameId_;
    std::chrono::steady_clock::time_point candidateStartTime_;
    static constexpr std::chrono::seconds HYSTERESIS_DURATION{3};

    // Platform-specific helpers
    GameInfo getCurrentForegroundGame() const;
};

} // namespace macroman
