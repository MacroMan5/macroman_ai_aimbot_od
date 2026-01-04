#include <catch2/catch_test_macros.hpp>
#include "config/GlobalConfig.h"
#include <filesystem>
#include <fstream>

using namespace macroman;
namespace fs = std::filesystem;

class TempINIFile {
public:
    TempINIFile(const std::string& content) {
        path_ = fs::temp_directory_path() / ("test_global_" + std::to_string(rand()) + ".ini");
        std::ofstream file(path_);
        file << content;
    }

    ~TempINIFile() {
        if (fs::exists(path_)) {
            fs::remove(path_);
        }
    }

    std::string path() const { return path_.string(); }

private:
    fs::path path_;
};

TEST_CASE("GlobalConfig - Load valid INI", "[config]") {
    const char* validINI = R"(
[Engine]
MaxFPS=120
VRAMBudget=256
LogLevel=Debug
ThreadAffinity=true

[UI]
OverlayHotkey=F1
ShowDebugInfo=false
Theme=Light

[IPC]
SharedMemoryName=CustomMemory
CommandPipeName=CustomPipe
)";

    TempINIFile tempFile(validINI);
    GlobalConfigManager manager;

    SECTION("Load and parse INI") {
        bool loaded = manager.loadFromFile(tempFile.path());
        REQUIRE(loaded == true);

        const auto& cfg = manager.getConfig();
        REQUIRE(cfg.maxFPS == 120);
        REQUIRE(cfg.vramBudgetMB == 256);
        REQUIRE(cfg.logLevel == "Debug");
        REQUIRE(cfg.threadAffinity == true);
        REQUIRE(cfg.overlayHotkey == "F1");
        REQUIRE(cfg.showDebugInfo == false);
        REQUIRE(cfg.theme == "Light");
        REQUIRE(cfg.sharedMemoryName == "CustomMemory");
        REQUIRE(cfg.commandPipeName == "CustomPipe");
    }
}

TEST_CASE("GlobalConfig - Validation", "[config]") {
    GlobalConfig config;

    SECTION("Valid configuration") {
        config.maxFPS = 144;
        config.vramBudgetMB = 512;
        config.logLevel = "Info";
        config.theme = "Dark";

        REQUIRE(config.validate() == true);
    }

    SECTION("Invalid FPS (too high)") {
        config.maxFPS = 500;
        config.vramBudgetMB = 512;
        config.logLevel = "Info";
        config.theme = "Dark";

        REQUIRE(config.validate() == false);
    }

    SECTION("Invalid log level") {
        config.maxFPS = 144;
        config.vramBudgetMB = 512;
        config.logLevel = "InvalidLevel";
        config.theme = "Dark";

        REQUIRE(config.validate() == false);
    }

    SECTION("Invalid theme") {
        config.maxFPS = 144;
        config.vramBudgetMB = 512;
        config.logLevel = "Info";
        config.theme = "Rainbow";

        REQUIRE(config.validate() == false);
    }
}

TEST_CASE("GlobalConfig - Defaults", "[config]") {
    GlobalConfig config;

    SECTION("Apply defaults for missing values") {
        config.maxFPS = 0;
        config.vramBudgetMB = 0;
        config.logLevel = "";
        config.theme = "";

        config.applyDefaults();

        REQUIRE(config.maxFPS == 144);
        REQUIRE(config.vramBudgetMB == 512);
        REQUIRE(config.logLevel == "Info");
        REQUIRE(config.theme == "Dark");
        REQUIRE(config.overlayHotkey == "HOME");
        REQUIRE(config.sharedMemoryName == "MacromanAimbot_Config");
    }
}

TEST_CASE("GlobalConfig - File not found", "[config]") {
    GlobalConfigManager manager;

    bool loaded = manager.loadFromFile("nonexistent_global.ini");
    REQUIRE(loaded == false);
    REQUIRE_FALSE(manager.getLastError().empty());
}
