#pragma once

#include "threading/ThreadManager.h"
#include "threading/LatestFrameQueue.h"
#include "capture/DuplicationCapture.h"
#include "detection/directml/DMLDetector.h" // Default to DML
#include "tracking/TargetTracker.h"
#include "input/InputManager.h"
#include "config/SharedConfigManager.h"
#include "config/GlobalConfig.h"
#include "ui/overlay/DebugOverlay.h"
#include "ui/backend/D3D11Backend.h"
#include "ui/backend/ImGuiBackend.h"
#include "core/metrics/PerformanceMetrics.h"

#include <memory>
#include <atomic>
#include <windows.h>

namespace macroman {

class Engine {
public:
    Engine();
    ~Engine();

    bool initialize();
    void run();
    void shutdown();

private:
    // Window procedure for overlay
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void handleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool createOverlayWindow();
    void toggleOverlayInteraction();  // P10-02: Toggle input passthrough

    // System Components
    std::unique_ptr<ThreadManager> threadManager_;
    std::unique_ptr<SharedConfigManager> sharedConfigManager_;
    
    // Core Pipeline Components
    std::unique_ptr<LatestFrameQueue<Frame>> detectionQueue_; // Capture -> Detection
    std::unique_ptr<LatestFrameQueue<DetectionBatch>> trackingQueue_; // Detection -> Tracking
    
    std::unique_ptr<IScreenCapture> capture_;
    std::unique_ptr<IDetector> detector_;
    std::unique_ptr<TargetTracker> tracker_;
    std::unique_ptr<InputManager> inputManager_; // Manages its own thread
    
    // UI / Overlay
    HWND overlayWindow_ = nullptr;
    std::unique_ptr<D3D11Backend> d3dBackend_;
    std::unique_ptr<ImGuiBackend> imguiBackend_;
    std::unique_ptr<DebugOverlay> debugOverlay_;

    // Thread Functions
    void captureLoop(std::atomic<bool>& stopFlag);
    void detectionLoop(std::atomic<bool>& stopFlag);
    void trackingLoop(std::atomic<bool>& stopFlag);
    // Input loop is managed by InputManager

    // State
    std::atomic<bool> running_{false};
    SharedConfig* sharedConfig_ = nullptr;
    bool overlayInteractionEnabled_{false};  // P10-02: Track passthrough state
    
    // Telemetry
    PerformanceMetrics metrics_;
};

} // namespace macroman
