#pragma once
#include <string>

namespace macroman {

struct GameProfile {
    std::string profileName = "Default";
    std::string processName = "";
    std::string windowTitle = "";

    // Input Scaling
    // How many pixels to move per mouse count.
    // Derived from: (Game Sensitivity * Mouse DPI * Yaw)
    // Universal calculator: 1.0 means "1 pixel on screen = 1 count" (perfect mapping)
    // Most games need adjustment.
    float sensitivity = 1.0f; 
    
    // Game Field of View (Horizontal)
    float fov = 90.0f;        
    
    // Aiming Offsets (Relative to Bounding Box)
    // 0.0 = Center
    // -0.5 = Top Edge
    // 0.5 = Bottom Edge
    // Default -0.2 is roughly "Neck/Head" area
    float aimOffsetY = -0.2f; 
    float aimOffsetX = 0.0f;
    
    // Trigger / Auto-Shoot
    bool autoShoot = false;
    float triggerDelay = 0.05f; // Seconds before shooting
    float triggerDuration = 0.1f; // Seconds to hold click
};

}
