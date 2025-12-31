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
