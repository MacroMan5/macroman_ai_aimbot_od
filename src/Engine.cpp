#include "Engine.h"
#include "core/utils/Logger.h"
#include "input/drivers/Win32Driver.h"
#include "input/humanization/Humanizer.h"
#include "input/movement/TrajectoryPlanner.h"
#include <iostream>
#include <mutex>
#include <format>

// Forward declare ImGui_ImplWin32_WndProcHandler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace macroman {

// Mutex for target snapshot (shared between Tracking thread and UI thread)
static std::mutex g_snapshotMutex;
static TargetSnapshot g_targetSnapshot;

Engine::Engine() = default;
Engine::~Engine() {
    shutdown();
}

bool Engine::initialize() {
    // P10-03: Enable high-DPI support (Per-Monitor V2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LOG_INFO("High-DPI awareness enabled (Per-Monitor V2)");

    // 1. Initialize Logging
    Logger::init("logs/macroman.log", spdlog::level::info);
    LOG_INFO("Initializing MacroMan AI Aimbot Engine...");

    // 2. Initialize SharedConfig
    sharedConfigManager_ = std::make_unique<SharedConfigManager>();
    if (!sharedConfigManager_->createMapping("MacromanAimbot_Config")) {
        LOG_CRITICAL("Failed to create SharedConfig mapping: {}", sharedConfigManager_->getLastError());
        return false;
    }
    sharedConfig_ = sharedConfigManager_->getConfig();
    LOG_INFO("SharedConfig initialized");

    // 3. Create Overlay Window
    if (!createOverlayWindow()) {
        LOG_CRITICAL("Failed to create overlay window");
        return false;
    }

    // 4. Initialize UI Backends
    d3dBackend_ = std::make_unique<D3D11Backend>();
    if (!d3dBackend_->initialize(overlayWindow_, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN))) {
        LOG_CRITICAL("Failed to initialize D3D11 Backend");
        return false;
    }

    imguiBackend_ = std::make_unique<ImGuiBackend>();
    if (!imguiBackend_->initialize(overlayWindow_, *d3dBackend_)) {
        LOG_CRITICAL("Failed to initialize ImGui Backend");
        return false;
    }

    debugOverlay_ = std::make_unique<DebugOverlay>();
    if (!debugOverlay_->initialize(overlayWindow_, d3dBackend_->getWidth(), d3dBackend_->getHeight())) {
        LOG_CRITICAL("Failed to initialize DebugOverlay");
        return false;
    }

    // 5. Initialize Components
    // Queues
    detectionQueue_ = std::make_unique<LatestFrameQueue<Frame>>();
    trackingQueue_ = std::make_unique<LatestFrameQueue<DetectionBatch>>();

    // Capture
    capture_ = std::make_unique<DuplicationCapture>();
    if (!capture_->initialize(nullptr)) {
        LOG_CRITICAL("Failed to initialize Capture: {}", capture_->getLastError());
        return false;
    }
    LOG_INFO("Capture initialized (DuplicationCapture)");

    // Detector (DML)
    std::string modelPath = "assets/models/sunxds_0.7.3.onnx"; 
    detector_ = std::make_unique<DMLDetector>();
    if (!detector_->initialize(modelPath)) {
        LOG_CRITICAL("Failed to initialize Detector with model: {}", modelPath);
        return false;
    }
    LOG_INFO("Detector initialized (DirectML)");

    // Tracker
    tracker_ = std::make_unique<TargetTracker>();
    LOG_INFO("Tracker initialized");

    // 6. Thread Manager
    threadManager_ = std::make_unique<ThreadManager>();

    return true;
}

void Engine::run() {
    if (running_) return;
    running_ = true;

    // Create Input Dependencies
    auto driver = std::make_shared<Win32Driver>();
    driver->initialize();
    
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();
    
    inputManager_ = std::make_unique<InputManager>(driver, planner, humanizer);

    // Start Threads via ThreadManager
    LOG_INFO("Starting threads...");
    
    // Windows Thread Priorities
    const int PRIORITY_TIME_CRITICAL = 15;
    const int PRIORITY_ABOVE_NORMAL = 1;
    const int PRIORITY_NORMAL = 0;

    threadManager_->createThread("CaptureThread", PRIORITY_TIME_CRITICAL, 
        [this](std::atomic<bool>& stopFlag) { this->captureLoop(stopFlag); });
        
    threadManager_->createThread("DetectionThread", PRIORITY_ABOVE_NORMAL, 
        [this](std::atomic<bool>& stopFlag) { this->detectionLoop(stopFlag); });
        
    threadManager_->createThread("TrackingThread", PRIORITY_NORMAL, 
        [this](std::atomic<bool>& stopFlag) { this->trackingLoop(stopFlag); });
    
    // InputManager starts its own thread
    inputManager_->start();

    // Set affinities (Phase 8) - Core 1, 2, 3
    threadManager_->setCoreAffinity(0, 1);
    threadManager_->setCoreAffinity(1, 2);
    threadManager_->setCoreAffinity(2, 3);

    LOG_INFO("Engine running");

    // Main Loop (UI)
    MSG msg = {};
    while (running_) {
        // Win32 Message Pump
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running_ = false;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        // Render Overlay
        if (sharedConfig_ && debugOverlay_ && d3dBackend_ && imguiBackend_) {
            imguiBackend_->beginFrame();
            d3dBackend_->beginFrame();

            auto metricsSnap = metrics_.snapshot();
            
            // Convert to TelemetrySnapshot
            TelemetrySnapshot telemetry;
            telemetry.captureFPS = metricsSnap.captureFPS;
            telemetry.captureLatency = metricsSnap.captureLatencyAvg;
            telemetry.detectionLatency = metricsSnap.detectionLatencyAvg;
            telemetry.trackingLatency = metricsSnap.trackingLatencyAvg;
            telemetry.inputLatency = metricsSnap.inputLatencyAvg;
            // Approximate end-to-end
            telemetry.endToEndLatency = telemetry.captureLatency + telemetry.detectionLatency + telemetry.trackingLatency + telemetry.inputLatency;
            
            telemetry.activeTargets = metricsSnap.activeTargets;
            telemetry.vramUsageMB = metricsSnap.vramUsageMB;
            
            telemetry.texturePoolStarved = metricsSnap.texturePoolStarved;
            telemetry.stalePredictionEvents = metricsSnap.stalePredictionEvents;
            telemetry.deadmanSwitchTriggered = metricsSnap.deadmanSwitchTriggered;
            
            // Get target snapshot safely
            TargetSnapshot targets;
            {
                std::lock_guard<std::mutex> lock(g_snapshotMutex);
                targets = g_targetSnapshot;
            }

            debugOverlay_->render(telemetry, targets, sharedConfig_);

            imguiBackend_->endFrame();
            d3dBackend_->endFrame();
        }

        // Periodic FPS update
        metrics_.updateFPS();

        // 60 FPS cap for UI
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    shutdown();
}

void Engine::shutdown() {
    running_ = false;
    LOG_INFO("Shutting down Engine...");

    if (inputManager_) inputManager_->stop();
    if (threadManager_) threadManager_->stopAll();
    
    if (detector_) detector_->release();
    if (capture_) capture_->shutdown();

    // Release backends
    imguiBackend_.reset();
    debugOverlay_.reset();
    d3dBackend_.reset();

    if (overlayWindow_) {
        ::DestroyWindow(overlayWindow_);
        ::UnregisterClass(TEXT("MacromanOverlay"), GetModuleHandle(NULL));
        overlayWindow_ = nullptr;
    }
    
    if (sharedConfigManager_) sharedConfigManager_->close();

    LOG_INFO("Engine shutdown complete");
}

void Engine::captureLoop(std::atomic<bool>& stopFlag) {
    LOG_INFO("Capture thread started");
    while (stopFlag) {
        auto start = std::chrono::high_resolution_clock::now();
        
        Frame frame = capture_->captureFrame();
        
        if (frame.isValid()) {
            auto* framePtr = new Frame(std::move(frame));
            detectionQueue_->push(framePtr);
            
            metrics_.recordCaptureLatency(std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count());
        } else {
             std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void Engine::detectionLoop(std::atomic<bool>& stopFlag) {
    LOG_INFO("Detection thread started");
    while (stopFlag) {
        Frame* frame = detectionQueue_->pop();
        if (frame) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Run detection
            auto detections = detector_->detect(*frame);
            
            // Create batch
            auto* batch = new DetectionBatch();
            // Copy detections to batch
            for (const auto& det : detections) {
                if (batch->observations.size() < 64) {
                    batch->observations.push_back(det);
                }
            }
            // Use captureTimeNs from frame instead of timestamp
            batch->captureTimeNs = frame->captureTimeNs; 

            trackingQueue_->push(batch);
            
            metrics_.recordDetectionLatency(std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count());
            
            delete frame; // RAII releases texture
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void Engine::trackingLoop(std::atomic<bool>& stopFlag) {
    LOG_INFO("Tracking thread started");
    while (stopFlag) {
        DetectionBatch* batch = trackingQueue_->pop();
        if (batch) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Update Tracker
            static auto lastTime = std::chrono::high_resolution_clock::now();
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;
            if (dt <= 0.0f) dt = 0.001f;

            tracker_->update(*batch, dt);
            
            // Get best target and send to input
            float fov = sharedConfig_->fov.load();
            Vec2 crosshair{960, 540}; // TODO: dynamic
            
            AimCommand cmd = tracker_->getAimCommand(crosshair, fov);
            inputManager_->updateAimCommand(cmd);

            // Update UI Snapshot
            {
                std::lock_guard<std::mutex> lock(g_snapshotMutex);
                const auto& db = tracker_->getDatabase();
                g_targetSnapshot.count = db.count;
                for(size_t i=0; i<db.count; ++i) {
                     g_targetSnapshot.bboxes[i] = db.bboxes[i];
                     g_targetSnapshot.confidences[i] = db.confidences[i];
                     g_targetSnapshot.hitboxTypes[i] = db.hitboxTypes[i];
                }
            }

            metrics_.recordTrackingLatency(std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count());
            metrics_.updateActiveTargets(tracker_->getDatabase().count);

            delete batch;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

// Window Procedure
LRESULT CALLBACK Engine::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Allow ImGui to process messages
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // P10-02: Handle INSERT key to toggle overlay interaction
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        // Get Engine instance (stored in window user data during creation)
        Engine* engine = reinterpret_cast<Engine*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (engine) {
            engine->toggleOverlayInteraction();
        }
        return 0;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_DPICHANGED: {
        // P10-03: Handle DPI change event
        // wParam contains new DPI (LOWORD = x, HIWORD = y)
        UINT newDpi = LOWORD(wParam);
        float scaleFactor = newDpi / 96.0f;  // 96 DPI is 100% scale

        LOG_INFO("DPI changed: {} (scale factor: {}x)", newDpi, scaleFactor);

        // Scale ImGui style
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiStyle defaultStyle;
        ImGui::StyleColorsDark(&defaultStyle);

        // Apply DPI scaling to style metrics
        style.WindowPadding = ImVec2(defaultStyle.WindowPadding.x * scaleFactor, defaultStyle.WindowPadding.y * scaleFactor);
        style.FramePadding = ImVec2(defaultStyle.FramePadding.x * scaleFactor, defaultStyle.FramePadding.y * scaleFactor);
        style.ItemSpacing = ImVec2(defaultStyle.ItemSpacing.x * scaleFactor, defaultStyle.ItemSpacing.y * scaleFactor);
        style.ItemInnerSpacing = ImVec2(defaultStyle.ItemInnerSpacing.x * scaleFactor, defaultStyle.ItemInnerSpacing.y * scaleFactor);
        style.ScrollbarSize = defaultStyle.ScrollbarSize * scaleFactor;
        style.GrabMinSize = defaultStyle.GrabMinSize * scaleFactor;

        // Suggested rect for new position/size (lParam)
        RECT* newRect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd, nullptr,
                     newRect->left, newRect->top,
                     newRect->right - newRect->left,
                     newRect->bottom - newRect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);

        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool Engine::createOverlayWindow() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, TEXT("MacromanOverlay"), NULL };
    RegisterClassEx(&wc);

    overlayWindow_ = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        TEXT("MacromanOverlay"), TEXT("Macroman Overlay"),
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, wc.hInstance, NULL
    );

    if (!overlayWindow_) return false;

    // P10-02: Store Engine instance pointer for WndProc access
    SetWindowLongPtr(overlayWindow_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Use LWA_COLORKEY for transparency (black pixels become transparent)
    SetLayeredWindowAttributes(overlayWindow_, RGB(0,0,0), 0, LWA_COLORKEY);

    ShowWindow(overlayWindow_, SW_SHOWDEFAULT);
    UpdateWindow(overlayWindow_);

    LOG_INFO("Overlay window created (passthrough mode: locked by default, press INSERT to toggle)");

    return true;
}

void Engine::toggleOverlayInteraction() {
    // P10-02: Toggle input passthrough for overlay
    overlayInteractionEnabled_ = !overlayInteractionEnabled_;

    LONG_PTR exStyle = GetWindowLongPtr(overlayWindow_, GWL_EXSTYLE);

    if (overlayInteractionEnabled_) {
        // Unlocked: Allow interaction with overlay (remove WS_EX_TRANSPARENT)
        exStyle &= ~WS_EX_TRANSPARENT;
        LOG_INFO("Overlay interaction ENABLED (can interact with UI)");
    } else {
        // Locked: Passthrough mode (add WS_EX_TRANSPARENT)
        exStyle |= WS_EX_TRANSPARENT;
        LOG_INFO("Overlay interaction DISABLED (click-through mode)");
    }

    SetWindowLongPtr(overlayWindow_, GWL_EXSTYLE, exStyle);
}

} // namespace macroman
