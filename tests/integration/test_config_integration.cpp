#include <catch2/catch_test_macros.hpp>
#include "config/ProfileManager.h"
#include "config/GameDetector.h"
#include "config/ModelManager.h"
#include "config/SharedConfig.h"
#include "config/SharedConfigManager.h"
#include "config/GlobalConfig.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace macroman;
using namespace std::chrono_literals;

TEST_CASE("Config Integration - Load profiles and switch games", "[integration]") {
    // Setup: Create temporary profiles directory
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "macroman_test_profiles_integration";
    if (std::filesystem::exists(tempDir)) {
        std::filesystem::remove_all(tempDir);
    }
    std::filesystem::create_directories(tempDir);

    // Write Valorant profile
    {
        std::ofstream file(tempDir / "valorant.json");
        file << R"({
            "gameId": "valorant",
            "displayName": "Valorant",
            "processNames": ["VALORANT.exe"],
            "windowTitles": ["VALORANT"],
            "detection": {
                "modelPath": "models/valorant_yolov8_640.onnx",
                "inputSize": [640, 640],
                "confidenceThreshold": 0.6,
                "nmsThreshold": 0.45,
                "hitboxMapping": {"0": "head", "1": "chest", "2": "body"}
            },
            "targeting": {
                "fov": 80.0,
                "smoothness": 0.65,
                "predictionStrength": 0.8,
                "hitboxPriority": ["head", "chest", "body"],
                "inputLatency": 12.5
            }
        })";
    }

    // Write CS2 profile
    {
        std::ofstream file(tempDir / "cs2.json");
        file << R"({
            "gameId": "cs2",
            "displayName": "Counter-Strike 2",
            "processNames": ["cs2.exe"],
            "windowTitles": ["Counter-Strike 2"],
            "detection": {
                "modelPath": "models/cs2_yolov8_640.onnx",
                "inputSize": [640, 640],
                "confidenceThreshold": 0.55,
                "nmsThreshold": 0.4,
                "hitboxMapping": {"0": "head", "1": "chest", "2": "body"}
            },
            "targeting": {
                "fov": 90.0,
                "smoothness": 0.7,
                "predictionStrength": 0.75,
                "hitboxPriority": ["head", "chest", "body"],
                "inputLatency": 15.0
            }
        })";
    }

    SECTION("Load multiple profiles") {
        ProfileManager manager;
        size_t loaded = manager.loadProfilesFromDirectory(tempDir.string());

        REQUIRE(loaded == 2);
        REQUIRE(manager.getProfileCount() == 2);

        auto* valorant = manager.getProfile("valorant");
        REQUIRE(valorant != nullptr);
        REQUIRE(valorant->displayName == "Valorant");
        REQUIRE(valorant->targeting.fov == 80.0f);

        auto* cs2 = manager.getProfile("cs2");
        REQUIRE(cs2 != nullptr);
        REQUIRE(cs2->displayName == "Counter-Strike 2");
        REQUIRE(cs2->targeting.fov == 90.0f);
    }

    SECTION("Model switching between games") {
        ProfileManager profileMgr;
        profileMgr.loadProfilesFromDirectory(tempDir.string());

        ModelManager modelMgr;

        // Create dummy model files for testing (must be > 1MB)
        if (!std::filesystem::exists("models")) {
            std::filesystem::create_directories("models");
        }
        std::string dummyData(1024 * 1024 + 1, 'X');
        {
            std::ofstream file("models/valorant_yolov8_640.onnx");
            file << dummyData;
        }
        {
            std::ofstream file("models/cs2_yolov8_640.onnx");
            file << dummyData;
        }

        // Switch to Valorant model
        auto* valorantProfile = profileMgr.getProfile("valorant");
        REQUIRE(valorantProfile != nullptr);

        bool loaded = modelMgr.loadModel(valorantProfile->detection.modelPath);
        REQUIRE(loaded == true);
        REQUIRE(modelMgr.hasModelLoaded() == true);

        std::string modelPath1 = modelMgr.getCurrentModelPath();
        REQUIRE(modelPath1.find("valorant_yolov8_640.onnx") != std::string::npos);

        // Switch to CS2 model
        auto* cs2Profile = profileMgr.getProfile("cs2");
        REQUIRE(cs2Profile != nullptr);

        loaded = modelMgr.switchModel(cs2Profile->detection.modelPath);
        REQUIRE(loaded == true);

        std::string modelPath2 = modelMgr.getCurrentModelPath();
        REQUIRE(modelPath2.find("cs2_yolov8_640.onnx") != std::string::npos);

        // Cleanup dummy models
        std::filesystem::remove("models/valorant_yolov8_640.onnx");
        std::filesystem::remove("models/cs2_yolov8_640.onnx");
    }

    // Cleanup
    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Config Integration - SharedConfig IPC", "[integration]") {
    SharedConfigManager manager;

    SECTION("Create and access shared memory") {
        bool created = manager.createMapping("MacromanTest_Config_Integration");
        REQUIRE(created == true);
        REQUIRE(manager.isActive() == true);

        auto* config = manager.getConfig();
        REQUIRE(config != nullptr);

        // Test hot-path writes (simulating Config UI)
        config->aimSmoothness.store(0.75f, std::memory_order_release);
        config->fov.store(100.0f, std::memory_order_release);
        config->enablePrediction.store(false, std::memory_order_release);

        // Test hot-path reads (simulating Engine)
        float smoothness = config->aimSmoothness.load(std::memory_order_acquire);
        float fov = config->fov.load(std::memory_order_acquire);
        bool prediction = config->enablePrediction.load(std::memory_order_acquire);

        REQUIRE(smoothness == 0.75f);
        REQUIRE(fov == 100.0f);
        REQUIRE(prediction == false);

                // Test telemetry writes (simulating Engine)

                config->captureFPS.store(144.5f, std::memory_order_release);

                config->activeTargets.store(3, std::memory_order_release);

        

                REQUIRE(config->captureFPS.load() == 144.5f);

                REQUIRE(config->activeTargets.load() == 3);

            }

        }

        

        TEST_CASE("Config Integration - Global and Profile synergy", "[integration]") {

            // This test verifies that we can load global settings and then profiles

            

            std::filesystem::path tempINI = std::filesystem::temp_directory_path() / "test_global_integration_syn.ini";

            {

                std::ofstream file(tempINI);

                file << R"(

        [Engine]

        MaxFPS=144

        VRAMBudget=512

        LogLevel=Info

        ThreadAffinity=true

        

        [UI]

        OverlayHotkey=HOME

        ShowDebugInfo=true

        Theme=Dark

        

        [IPC]

        SharedMemoryName=MacromanAimbot_IntegrationTest_Syn

        CommandPipeName=MacromanAimbot_Commands

        )";

            }

        

            GlobalConfigManager globalMgr;

            REQUIRE(globalMgr.loadFromFile(tempINI.string()) == true);

            

            SharedConfigManager sharedMgr;

            REQUIRE(sharedMgr.createMapping(globalMgr.getConfig().sharedMemoryName) == true);

            

            // reset() is not called in createMapping, but SharedConfig has default values in declaration.

            // Actually createMapping in SharedConfigManager calls memset 0 or initialize? Let's check reset.

            // Looking at SharedConfig.h, fields have initializers.

            

            REQUIRE(sharedMgr.getConfig()->fov.load() == 80.0f); // Default from SharedConfig initialization

            

            std::filesystem::remove(tempINI);

        }

        