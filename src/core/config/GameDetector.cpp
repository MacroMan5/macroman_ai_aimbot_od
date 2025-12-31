#include "GameDetector.h"
#include "ProfileManager.h"
#include "utils/Logger.h"
#include <Windows.h>
#include <Psapi.h>

namespace macroman {

GameDetector::GameDetector() = default;

void GameDetector::setProfileManager(const ProfileManager* manager) {
    profileManager_ = manager;
}

void GameDetector::setGameChangedCallback(GameChangedCallback callback) {
    gameChangedCallback_ = std::move(callback);
}

void GameDetector::poll() {
    if (!profileManager_) {
        LOG_ERROR("GameDetector: ProfileManager not set");
        return;
    }

    // Get current foreground game
    GameInfo gameInfo = getCurrentForegroundGame();

    if (gameInfo.processName.empty()) {
        // No foreground window or failed to get process name
        candidateGameId_.reset();
        return;
    }

    // Try to find matching profile
    auto* profile = profileManager_->findProfileByProcess(
        gameInfo.processName,
        gameInfo.windowTitle
    );

    if (!profile) {
        // No matching profile - reset candidate
        candidateGameId_.reset();
        return;
    }

    // Check if this is a new candidate or same as before
    if (!candidateGameId_.has_value() || candidateGameId_.value() != profile->gameId) {
        // New game detected - start hysteresis timer
        candidateGameId_ = profile->gameId;
        candidateStartTime_ = std::chrono::steady_clock::now();

        LOG_INFO("GameDetector: New candidate detected: {} (waiting {}s for stability)",
                 profile->gameId, HYSTERESIS_DURATION.count());
        return;
    }

    // Same candidate - check if hysteresis period elapsed
    auto elapsed = std::chrono::steady_clock::now() - candidateStartTime_;

    if (elapsed >= HYSTERESIS_DURATION) {
        // Stable for required duration - fire callback
        LOG_INFO("GameDetector: Game confirmed: {} (stable for {}s)",
                 profile->gameId, HYSTERESIS_DURATION.count());

        if (gameChangedCallback_) {
            gameChangedCallback_(*profile, gameInfo);
        }

        // Reset candidate to avoid repeated callbacks
        candidateGameId_.reset();
    }
}

std::optional<std::string> GameDetector::getCandidateGameId() const {
    return candidateGameId_;
}

int64_t GameDetector::getHysteresisTimeRemaining() const {
    if (!candidateGameId_.has_value()) {
        return 0;
    }

    auto elapsed = std::chrono::steady_clock::now() - candidateStartTime_;
    auto remaining = HYSTERESIS_DURATION - elapsed;

    if (remaining.count() <= 0) {
        return 0;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
}

bool GameDetector::isInHysteresis() const {
    return candidateGameId_.has_value() && getHysteresisTimeRemaining() > 0;
}

GameInfo GameDetector::getCurrentForegroundGame() const {
    GameInfo info{};

    // Get foreground window
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return info;
    }

    info.windowHandle = hwnd;

    // Get window title
    char windowTitle[256] = {0};
    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
    info.windowTitle = windowTitle;

    // Get process ID
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == 0) {
        return info;
    }

    // Open process to get executable name
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return info;
    }

    // Get process name (executable filename)
    char processName[MAX_PATH] = {0};
    DWORD size = sizeof(processName);

    if (GetModuleBaseNameA(hProcess, nullptr, processName, size)) {
        info.processName = processName;
    }

    CloseHandle(hProcess);

    return info;
}

} // namespace macroman
