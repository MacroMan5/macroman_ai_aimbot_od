#include <catch2/catch_test_macros.hpp>
#include "config/ProfileManager.h"
#include <filesystem>
#include <fstream>

using namespace macroman;
namespace fs = std::filesystem;

// Test helper: create temporary JSON file
class TempJsonFile {
public:
    TempJsonFile(const std::string& content) {
        path_ = fs::temp_directory_path() / ("test_profile_" + std::to_string(rand()) + ".json");
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

TEST_CASE("ProfileManager - Load valid profile", "[config]") {
    const char* validJson = R"({
        "gameId": "test_game",
        "displayName": "Test Game",
        "processNames": ["testgame.exe"],
        "windowTitles": ["Test Game Window"],
        "detection": {
            "modelPath": "models/test.onnx",
            "inputSize": [640, 640],
            "confidenceThreshold": 0.6,
            "nmsThreshold": 0.45,
            "hitboxMapping": {
                "0": "head",
                "1": "body"
            }
        },
        "targeting": {
            "fov": 80.0,
            "smoothness": 0.65,
            "predictionStrength": 0.8,
            "hitboxPriority": ["head", "body"],
            "inputLatency": 12.5
        }
    })";

    TempJsonFile tempFile(validJson);
    ProfileManager manager;

    SECTION("Load single profile") {
        bool loaded = manager.loadProfile(tempFile.path());
        REQUIRE(loaded == true);
        REQUIRE(manager.getProfileCount() == 1);

        auto* profile = manager.getProfile("test_game");
        REQUIRE(profile != nullptr);
        REQUIRE(profile->displayName == "Test Game");
        REQUIRE(profile->detection.modelPath == "models/test.onnx");
        REQUIRE(profile->targeting.fov == 80.0f);
    }
}

TEST_CASE("ProfileManager - Invalid JSON handling", "[config]") {
    ProfileManager manager;

    SECTION("Missing required field (gameId)") {
        const char* invalidJson = R"({
            "displayName": "Test Game",
            "processNames": ["test.exe"],
            "windowTitles": [],
            "detection": {
                "modelPath": "models/test.onnx",
                "inputSize": [640, 640],
                "confidenceThreshold": 0.6,
                "nmsThreshold": 0.45,
                "hitboxMapping": {}
            },
            "targeting": {
                "fov": 80.0,
                "smoothness": 0.65,
                "predictionStrength": 0.8,
                "hitboxPriority": ["head"],
                "inputLatency": 12.5
            }
        })";

        TempJsonFile tempFile(invalidJson);
        bool loaded = manager.loadProfile(tempFile.path());

        REQUIRE(loaded == false);
        REQUIRE(manager.getProfileCount() == 0);
        REQUIRE_FALSE(manager.getLastError().empty());
    }

    SECTION("Invalid confidence threshold (>1.0)") {
        const char* invalidJson = R"({
            "gameId": "test_game",
            "displayName": "Test Game",
            "processNames": ["test.exe"],
            "windowTitles": [],
            "detection": {
                "modelPath": "models/test.onnx",
                "inputSize": [640, 640],
                "confidenceThreshold": 1.5,
                "nmsThreshold": 0.45,
                "hitboxMapping": {}
            },
            "targeting": {
                "fov": 80.0,
                "smoothness": 0.65,
                "predictionStrength": 0.8,
                "hitboxPriority": ["head"],
                "inputLatency": 12.5
            }
        })";

        TempJsonFile tempFile(invalidJson);
        bool loaded = manager.loadProfile(tempFile.path());

        REQUIRE(loaded == false);
        REQUIRE(manager.getLastError().find("confidenceThreshold") != std::string::npos);
    }

    SECTION("Malformed JSON") {
        const char* malformedJson = R"({
            "gameId": "test",
            "displayName": "Test",
            // This is invalid JSON (comment not allowed)
            "processNames": ["test.exe"]
        })";

        TempJsonFile tempFile(malformedJson);
        bool loaded = manager.loadProfile(tempFile.path());

        REQUIRE(loaded == false);
        REQUIRE(manager.getLastError().find("JSON parse error") != std::string::npos);
    }

    SECTION("File not found") {
        bool loaded = manager.loadProfile("nonexistent_file.json");
        REQUIRE(loaded == false);
        REQUIRE(manager.getLastError().find("not found") != std::string::npos);
    }
}

TEST_CASE("ProfileManager - Profile matching", "[config]") {
    const char* profileJson = R"({
        "gameId": "valorant",
        "displayName": "Valorant",
        "processNames": ["VALORANT.exe", "VALORANT-Win64-Shipping.exe"],
        "windowTitles": ["VALORANT"],
        "detection": {
            "modelPath": "models/valorant.onnx",
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

    SECTION("Find by process name") {
        auto* profile = manager.findProfileByProcess("VALORANT.exe");
        REQUIRE(profile != nullptr);
        REQUIRE(profile->gameId == "valorant");
    }

    SECTION("Find by alternate process name") {
        auto* profile = manager.findProfileByProcess("VALORANT-Win64-Shipping.exe");
        REQUIRE(profile != nullptr);
        REQUIRE(profile->gameId == "valorant");
    }

    SECTION("Find by window title") {
        auto* profile = manager.findProfileByProcess("", "VALORANT - Main Menu");
        REQUIRE(profile != nullptr);
        REQUIRE(profile->gameId == "valorant");
    }

    SECTION("No match returns nullptr") {
        auto* profile = manager.findProfileByProcess("notepad.exe");
        REQUIRE(profile == nullptr);
    }
}
