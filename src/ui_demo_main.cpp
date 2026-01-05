/**
 * @file ui_demo_main.cpp
 * @brief Standalone UI Demo Application (P10-04)
 *
 * Purpose: Test UI rendering without full Engine/Detection pipeline.
 * Features:
 * - D3D11Backend + ImGuiBackend + DebugOverlay
 * - Fake telemetry data (sine waves) for visualization
 * - Validates overlay rendering, theming, and profiler graphs
 *
 * Build: macroman_ui_demo.exe
 * Usage: Run standalone to verify UI works before integration testing
 */

#include "ui/backend/D3D11Backend.h"
#include "ui/backend/ImGuiBackend.h"
#include "ui/overlay/DebugOverlay.h"
#include "core/config/SharedConfig.h"
#include <Windows.h>
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

using namespace macroman;

// Window constants
constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1080;
constexpr const char* WINDOW_TITLE = "MacroMan UI Demo - Phase 10";

// Global state
HWND g_hwnd = nullptr;
bool g_running = true;

/**
 * @brief Generate fake telemetry data with sine waves for visualization
 * @param time Elapsed time in seconds
 * @return Fake telemetry snapshot
 */
TelemetrySnapshot generateFakeTelemetry(float time) {
    TelemetrySnapshot telemetry;

    // Simulate varying FPS (144 +/- 20)
    telemetry.captureFPS = 144.0f + 20.0f * std::sin(time * 0.5f);

    // Simulate latencies with sine waves (different frequencies)
    telemetry.captureLatency = 1.0f + 0.5f * std::sin(time * 2.0f);
    telemetry.detectionLatency = 7.0f + 2.0f * std::sin(time * 1.5f);
    telemetry.trackingLatency = 0.8f + 0.3f * std::sin(time * 3.0f);
    telemetry.inputLatency = 0.3f + 0.2f * std::sin(time * 4.0f);
    telemetry.endToEndLatency = telemetry.captureLatency + telemetry.detectionLatency +
                                 telemetry.trackingLatency + telemetry.inputLatency;

    // Simulate active targets (0-8)
    telemetry.activeTargets = static_cast<int>(4.0f + 4.0f * std::sin(time * 0.8f));

    // Simulate VRAM usage (200-400 MB)
    telemetry.vramUsageMB = static_cast<size_t>(300 + 100 * std::sin(time * 0.3f));

    // Safety metrics (rarely triggered)
    telemetry.texturePoolStarved = (time > 10.0f && std::fmod(time, 15.0f) < 0.5f) ? 1 : 0;
    telemetry.stalePredictionEvents = static_cast<uint64_t>(time * 0.1f);
    telemetry.deadmanSwitchTriggered = (time > 20.0f && std::fmod(time, 30.0f) < 0.5f) ? 1 : 0;

    telemetry.timestamp = std::chrono::system_clock::now();

    return telemetry;
}

/**
 * @brief Generate fake target bounding boxes for visualization
 * @param time Elapsed time in seconds
 * @return Fake target snapshot
 */
TargetSnapshot generateFakeTargets(float time) {
    TargetSnapshot targets;

    // Generate 2-6 targets
    targets.count = static_cast<size_t>(4 + 2 * std::sin(time * 0.6f));
    targets.count = std::min(targets.count, TargetSnapshot::MAX_TARGETS);

    for (size_t i = 0; i < targets.count; ++i) {
        // Spread targets across screen with circular motion
        float angle = (time * 0.5f) + (i * 2.0f * 3.14159f / targets.count);
        float radius = 200.0f + 100.0f * std::sin(time * 0.3f + i);

        float centerX = WINDOW_WIDTH / 2.0f + radius * std::cos(angle);
        float centerY = WINDOW_HEIGHT / 2.0f + radius * std::sin(angle);

        float width = 80.0f + 20.0f * std::sin(time * 2.0f + i);
        float height = 120.0f + 30.0f * std::sin(time * 1.8f + i);

        targets.bboxes[i] = BBox{
            centerX - width / 2.0f,
            centerY - height / 2.0f,
            width,
            height
        };

        targets.confidences[i] = 0.85f + 0.15f * std::sin(time * 3.0f + i);

        // Rotate hitbox types
        HitboxType types[] = {HitboxType::Head, HitboxType::Chest, HitboxType::Body};
        targets.hitboxTypes[i] = types[i % 3];
    }

    // Select first target as "selected"
    targets.selectedTargetIndex = 0;

    return targets;
}

/**
 * @brief Window procedure for demo window
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                PostQuitMessage(0);
            }
            return 0;

        case WM_SIZE:
            // Handle resize (would call backend.resize() if needed)
            return 0;

        case WM_DPICHANGED: {
            // P10-03: Handle DPI change event
            UINT newDpi = LOWORD(wParam);
            float scaleFactor = newDpi / 96.0f;

            spdlog::info("DPI changed: {} (scale factor: {}x)", newDpi, scaleFactor);

            // Scale ImGui style
            ImGuiStyle& style = ImGui::GetStyle();
            ImGuiStyle defaultStyle;
            ImGui::StyleColorsDark(&defaultStyle);

            style.WindowPadding = ImVec2(defaultStyle.WindowPadding.x * scaleFactor, defaultStyle.WindowPadding.y * scaleFactor);
            style.FramePadding = ImVec2(defaultStyle.FramePadding.x * scaleFactor, defaultStyle.FramePadding.y * scaleFactor);
            style.ItemSpacing = ImVec2(defaultStyle.ItemSpacing.x * scaleFactor, defaultStyle.ItemSpacing.y * scaleFactor);
            style.ItemInnerSpacing = ImVec2(defaultStyle.ItemInnerSpacing.x * scaleFactor, defaultStyle.ItemInnerSpacing.y * scaleFactor);
            style.ScrollbarSize = defaultStyle.ScrollbarSize * scaleFactor;
            style.GrabMinSize = defaultStyle.GrabMinSize * scaleFactor;

            // Suggested rect for new position/size
            RECT* newRect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr,
                         newRect->left, newRect->top,
                         newRect->right - newRect->left,
                         newRect->bottom - newRect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

/**
 * @brief Create demo window
 * @return Window handle or nullptr on failure
 */
HWND createDemoWindow() {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "MacroManUIDemoClass";

    if (!RegisterClassEx(&wc)) {
        spdlog::error("Failed to register window class");
        return nullptr;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;

    RECT windowRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

    HWND hwnd = CreateWindowEx(
        exStyle,
        wc.lpszClassName,
        WINDOW_TITLE,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    if (!hwnd) {
        spdlog::error("Failed to create window");
        return nullptr;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return hwnd;
}

/**
 * @brief Main entry point for UI demo
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // P10-03: Enable high-DPI support (Per-Monitor V2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("MacroMan UI Demo - Phase 10 starting...");

    // Create window
    g_hwnd = createDemoWindow();
    if (!g_hwnd) {
        spdlog::error("Failed to create demo window");
        return 1;
    }

    // Initialize D3D11 backend
    D3D11Backend d3dBackend;
    if (!d3dBackend.initialize(g_hwnd, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        spdlog::error("Failed to initialize D3D11 backend");
        return 1;
    }
    spdlog::info("D3D11 backend initialized");

    // Initialize ImGui backend
    ImGuiBackend imguiBackend;
    if (!imguiBackend.initialize(g_hwnd, d3dBackend)) {
        spdlog::error("Failed to initialize ImGui backend");
        return 1;
    }
    imguiBackend.setTheme();  // P10-01: Apply Cyberpunk theme
    spdlog::info("ImGui backend initialized with Cyberpunk theme");

    // Initialize Debug Overlay
    DebugOverlay overlay;
    if (!overlay.initialize(g_hwnd, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        spdlog::error("Failed to initialize debug overlay");
        return 1;
    }
    spdlog::info("Debug overlay initialized");

    // Configure overlay (enable all features for demo)
    DebugOverlay::Config overlayConfig;
    overlayConfig.enabled = true;
    overlayConfig.showMetrics = true;
    overlayConfig.showBBoxes = true;
    overlayConfig.showComponentToggles = true;
    overlayConfig.showSafetyMetrics = true;  // Show for demo
    overlayConfig.showProfiler = true;
    overlayConfig.overlayAlpha = 0.9f;
    overlay.updateConfig(overlayConfig);

    // Create fake shared config
    SharedConfig sharedConfig;
    sharedConfig.reset();
    sharedConfig.enableAiming.store(true);
    sharedConfig.enableTracking.store(true);
    sharedConfig.enablePrediction.store(true);
    sharedConfig.enableTremor.store(true);

    // Timing
    auto startTime = std::chrono::steady_clock::now();
    DWORD lastFrameTime = GetTickCount();
    float frameTime = 0.0f;

    spdlog::info("Entering render loop (Press ESC to exit)");

    // Main loop
    MSG msg = {};
    while (g_running) {
        // Process messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Calculate elapsed time
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - startTime).count();

        // Generate fake data
        TelemetrySnapshot telemetry = generateFakeTelemetry(elapsedTime);
        TargetSnapshot targets = generateFakeTargets(elapsedTime);

        // Begin frame
        d3dBackend.beginFrame();
        imguiBackend.beginFrame();

        // Render overlay
        overlay.render(telemetry, targets, &sharedConfig);

        // Show demo window for ImGui testing
        ImGui::ShowDemoWindow();

        // End frame
        imguiBackend.endFrame();
        d3dBackend.endFrame();

        // Frame timing
        DWORD currentTime = GetTickCount();
        frameTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        // Limit to ~60 FPS for demo
        Sleep(16);
    }

    spdlog::info("Shutting down UI demo...");

    // Cleanup
    overlay.shutdown();
    imguiBackend.shutdown();
    d3dBackend.shutdown();

    spdlog::info("UI demo shutdown complete");

    return 0;
}
