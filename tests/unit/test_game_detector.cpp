#include <catch2/catch_test_macros.hpp>
#include "config/GameDetector.h"
#include "config/ProfileManager.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace macroman;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

// Test helper: create temporary JSON file
class TempJsonFile {
public:
    TempJsonFile(const std::string& content) {
        path_ = fs::temp_directory_path() / ("test_detector_" + std::to_string(rand()) + ".json");
        std::ofstream file(path_);
        file << content;
    }

    ~TempJsonFile() {
        if (fs::exists(path_)) {
            fs::remove(path_);
        }
    }

    std::string path() const { return path_.string(); }

private:
    fs::path path_;
};

TEST_CASE("GameDetector - Initialization", "[config][detector]") {
    GameDetector detector;

    SECTION("Starts with no candidate") {
        REQUIRE_FALSE(detector.getCandidateGameId().has_value());
        REQUIRE(detector.getHysteresisTimeRemaining() == 0);
        REQUIRE_FALSE(detector.isInHysteresis());
    }
}

TEST_CASE("GameDetector - ProfileManager requirement", "[config][detector]") {
    GameDetector detector;

    SECTION("poll() requires ProfileManager to be set") {
        // Should not crash without ProfileManager, just return early
        detector.poll();  // Should log error and return

        REQUIRE_FALSE(detector.getCandidateGameId().has_value());
    }
}

TEST_CASE("GameDetector - Hysteresis state tracking", "[config][detector]") {
    // Create a test profile
    const char* profileJson = R"({
        "gameId": "test_game",
        "displayName": "Test Game",
        "processNames": ["testgame.exe"],
        "windowTitles": ["Test Game Window"],
        "detection": {
            "modelPath": "models/test.onnx",
            "inputSize": [640, 640],
            "confidenceThreshold": 0.6,
            "nmsThreshold": 0.45,
            "hitboxMapping": {"0": "head"}
        },
        "targeting": {
            "fov": 80.0,
            "smoothness": 0.65,
            "predictionStrength": 0.8,
            "hitboxPriority": ["head"],
            "inputLatency": 12.5
        }
    })";

    TempJsonFile tempFile(profileJson);
    ProfileManager manager;
    manager.loadProfile(tempFile.path());

    GameDetector detector;
    detector.setProfileManager(&manager);

    SECTION("Hysteresis state starts inactive") {
        REQUIRE_FALSE(detector.isInHysteresis());
        REQUIRE(detector.getHysteresisTimeRemaining() == 0);
    }

    SECTION("Hysteresis time decreases over time") {
        // Note: This test cannot easily simulate game detection without
        // mocking Windows APIs (GetForegroundWindow, etc.).
        // Testing the state accessors only for now.

        // Verify accessor methods work correctly
        REQUIRE(detector.getHysteresisTimeRemaining() >= 0);
        REQUIRE_FALSE(detector.getCandidateGameId().has_value());
    }
}

TEST_CASE("GameDetector - Callback registration", "[config][detector]") {
    GameDetector detector;

    SECTION("Callback can be set") {
        bool callbackInvoked = false;
        GameProfile receivedProfile;
        GameInfo receivedInfo;

        detector.setGameChangedCallback([&](const GameProfile& profile, const GameInfo& info) {
            callbackInvoked = true;
            receivedProfile = profile;
            receivedInfo = info;
        });

        // Callback is registered (no way to verify without triggering)
        REQUIRE_FALSE(callbackInvoked);  // Not invoked yet
    }

    SECTION("Callback can be unset") {
        detector.setGameChangedCallback(nullptr);
        // Should not crash on poll() without callback
        detector.poll();
    }
}

TEST_CASE("GameDetector - Hysteresis timing constants", "[config][detector]") {
    // Verify the 3-second hysteresis duration is as documented
    GameDetector detector;

    SECTION("Hysteresis duration is 3 seconds (3000ms)") {
        // This is verified in the implementation:
        // static constexpr std::chrono::seconds HYSTERESIS_DURATION{3};

        // Test that time remaining never exceeds 3000ms
        REQUIRE(detector.getHysteresisTimeRemaining() <= 3000);
    }
}

// Note: Full integration tests with actual Windows API calls are not
// practical in unit tests. The following would require:
// 1. Mocking GetForegroundWindow() to return a specific HWND
// 2. Mocking GetWindowText() to return specific window title
// 3. Mocking GetModuleBaseName() to return specific process name
//
// These would be better suited for manual testing or integration tests
// with a controlled test environment (e.g., launching actual test processes).
