#pragma once

#include "GameProfile.h"
#include "../core/interfaces/IDetector.h" // For DetectorConfig
#include "../input/movement/TrajectoryPlanner.h" // For TrajectoryConfig
#include <vector>

namespace macroman {

struct AppConfig {
    // Sub-module configs
    DetectorConfig detector;
    TrajectoryConfig trajectory;
    
    // Current Game Settings
    GameProfile currentGame;
    
    // Available profiles
    std::vector<GameProfile> profiles;

    // System Settings
    int captureMonitorIndex = 0;
    int targetFps = 144;
    bool showOverlay = true;
    bool debugMode = false;
    
    // Capture Method: "duplication" or "winrt"
    std::string captureMethod = "winrt"; 
    
    // Detector Backend: "dml" or "tensorrt"
    std::string detectorBackend = "dml"; 
};

class ConfigManager {
public:
    static AppConfig& instance() {
        static AppConfig config;
        return config;
    }
    
    // TODO: Implement load/save logic (JSON/INI)
    void load(const std::string& path);
    void save(const std::string& path);
};

}
