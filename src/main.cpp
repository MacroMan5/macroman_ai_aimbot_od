#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "capture/WinrtCapture.h"
#include "tracking/KalmanPredictor.h"
#include "detection/DetectorFactory.h"
#include "core/types/Enums.h"

int main() {
    std::cout << "[Main] Starting Macroman AI Aimbot..." << std::endl;

    // 1. Initialize Capture
    std::cout << "[Main] Instantiating Capture..." << std::endl;
    std::unique_ptr<sunone::IScreenCapture> capture = std::make_unique<sunone::WinrtCapture>();
    
    if (capture->isAvailable()) {
        std::cout << "[Main] Capture method is available." << std::endl;
    } else {
        std::cerr << "[Main] Capture method not available on this system." << std::endl;
    }

    // 2. Initialize Detector
    std::cout << "[Main] Checking Detector Backends..." << std::endl;
    auto backends = sunone::DetectorFactory::getAvailableBackends();
    for (auto type : backends) {
        std::cout << " - " << sunone::DetectorFactory::getTypeName(type) << std::endl;
    }

    std::cout << "[Main] Build Check Complete." << std::endl;
    return 0;
}
