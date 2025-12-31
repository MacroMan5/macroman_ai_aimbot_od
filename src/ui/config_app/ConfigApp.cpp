#include "../../core/config/SharedConfigManager.h"
#include "../../core/config/SharedConfig.h"
#include "../../core/utils/Logger.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <tchar.h>
#include <chrono>

namespace macroman {

/**
 * @brief External Configuration UI Application
 *
 * Standalone ImGui application for live tuning and telemetry visualization.
 *
 * Features:
 * - Live tuning sliders (smoothness, FOV, config atomics)
 * - Telemetry dashboard (FPS, latency, VRAM, safety metrics)
 * - Component toggles (enable aiming, tracking, prediction, tremor)
 *
 * Architecture:
 * - Connects to Engine's SharedConfig via memory-mapped file
 * - Lock-free atomic reads/writes (no mutexes)
 * - 60 FPS UI refresh rate
 *
 * Window Specifications:
 * - Size: 1200x800 (standard windowed app, not transparent)
 * - Rendering: DirectX 11 + Dear ImGui 1.91.2
 * - No screenshot protection (not needed for external UI)
 */
class ConfigApp {
public:
    bool initialize();
    void run();
    void shutdown();

private:
    void renderUI();
    void renderLiveTuning();
    void renderTelemetryDashboard();
    void renderComponentToggles();
    void renderSafetyMetrics();

    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void createRenderTarget();
    void cleanupRenderTarget();

    // DirectX 11
    ID3D11Device* d3dDevice_ = nullptr;
    ID3D11DeviceContext* d3dDeviceContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* mainRenderTargetView_ = nullptr;

    // IPC
    SharedConfigManager configManager_;
    SharedConfig* sharedConfig_ = nullptr;

    // Window
    HWND hwnd_ = nullptr;
    bool running_ = true;

    // UI state
    float lastFPS_ = 0.0f;
    std::chrono::steady_clock::time_point lastUpdate_;
};

}  // namespace macroman

// Forward declarations for Win32 (global namespace)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            // Handle window resize if needed
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

namespace macroman {

bool ConfigApp::initialize() {
    // Initialize logger
    Logger::init();
    LOG_INFO("ConfigApp starting...");

    // Create application window
    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"MacromanConfigUI", nullptr
    };
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowW(
        wc.lpszClassName,
        L"Macroman AI Aimbot - Configuration",
        WS_OVERLAPPEDWINDOW,
        100, 100,           // Position
        1200, 800,          // Size
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hwnd_) {
        LOG_ERROR("Failed to create window");
        return false;
    }

    // Initialize DirectX 11
    if (!createDeviceD3D(hwnd_)) {
        cleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // Show window
    ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd_);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3dDevice_, d3dDeviceContext_);

    // Connect to Engine's SharedConfig
    if (!configManager_.openMapping("MacromanAimbot_Config")) {
        LOG_WARN("Failed to connect to Engine SharedConfig: {}", configManager_.getLastError());
        LOG_WARN("Is the engine running? UI will start but config will be unavailable.");
        // Don't fail - allow UI to start and try reconnecting
    } else {
        sharedConfig_ = configManager_.getConfig();
        LOG_INFO("Connected to Engine SharedConfig");
    }

    lastUpdate_ = std::chrono::steady_clock::now();
    LOG_INFO("ConfigApp initialized successfully");
    return true;
}

void ConfigApp::run() {
    MSG msg = {};
    while (running_ && msg.message != WM_QUIT) {
        // Poll and handle messages
        if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render UI
        renderUI();

        // Rendering
        ImGui::Render();
        constexpr float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        d3dDeviceContext_->OMSetRenderTargets(1, &mainRenderTargetView_, nullptr);
        d3dDeviceContext_->ClearRenderTargetView(mainRenderTargetView_, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        swapChain_->Present(1, 0); // VSync on (60 FPS)
    }
}

void ConfigApp::shutdown() {
    LOG_INFO("ConfigApp shutting down...");

    // Cleanup ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Cleanup DirectX
    cleanupDeviceD3D();

    // Cleanup window
    ::DestroyWindow(hwnd_);
    ::UnregisterClassW(L"MacromanConfigUI", GetModuleHandle(nullptr));

    // Close SharedConfig connection
    configManager_.close();

    LOG_INFO("ConfigApp shutdown complete");
}

void ConfigApp::renderUI() {
    // Main window (fullscreen within client area)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Macroman AI Aimbot - Configuration UI",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::Text("Macroman AI Aimbot - External Configuration UI");
    ImGui::Separator();

    // Connection status
    if (sharedConfig_) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Connected to Engine");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: NOT CONNECTED (Engine not running?)");
        if (ImGui::Button("Retry Connection")) {
            if (configManager_.openMapping("MacromanAimbot_Config")) {
                sharedConfig_ = configManager_.getConfig();
                LOG_INFO("Reconnected to Engine SharedConfig");
            } else {
                LOG_ERROR("Failed to reconnect: {}", configManager_.getLastError());
            }
        }
        ImGui::End();
        return; // Don't render rest of UI if not connected
    }

    ImGui::Separator();

    // Layout: Two columns
    ImGui::Columns(2, "main_columns", true);

    // Left column: Live Tuning + Component Toggles
    renderLiveTuning();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    renderComponentToggles();

    // Right column: Telemetry Dashboard + Safety Metrics
    ImGui::NextColumn();
    renderTelemetryDashboard();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    renderSafetyMetrics();

    ImGui::Columns(1);
    ImGui::End();
}

void ConfigApp::renderLiveTuning() {
    ImGui::Text("Live Tuning");
    ImGui::Separator();

    if (!sharedConfig_) return;

    // Smoothness slider
    float smoothness = sharedConfig_->aimSmoothness.load(std::memory_order_relaxed);
    if (ImGui::SliderFloat("Aim Smoothness", &smoothness, 0.0f, 1.0f, "%.2f")) {
        sharedConfig_->aimSmoothness.store(smoothness, std::memory_order_relaxed);
        LOG_INFO("Smoothness updated: {:.2f}", smoothness);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("0.0 = Instant (robotic), 1.0 = Very smooth (human-like)");
    }

    // FOV slider
    float fov = sharedConfig_->fov.load(std::memory_order_relaxed);
    if (ImGui::SliderFloat("Field of View (FOV)", &fov, 10.0f, 180.0f, "%.0f deg")) {
        sharedConfig_->fov.store(fov, std::memory_order_relaxed);
        LOG_INFO("FOV updated: {:.1f} degrees", fov);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Targeting area (degrees). Smaller = more precise.");
    }

    // Active Profile ID (read-only for now, future: profile selector)
    uint32_t profileId = sharedConfig_->activeProfileId.load(std::memory_order_relaxed);
    ImGui::Text("Active Profile ID: %u", profileId);
}

void ConfigApp::renderComponentToggles() {
    ImGui::Text("Component Toggles");
    ImGui::Separator();

    if (!sharedConfig_) return;

    // Aiming toggle
    bool enableAiming = sharedConfig_->enableAiming.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Enable Aiming", &enableAiming)) {
        sharedConfig_->enableAiming.store(enableAiming, std::memory_order_relaxed);
        LOG_INFO("Aiming {}", enableAiming ? "enabled" : "disabled");
    }

    // Tracking toggle
    bool enableTracking = sharedConfig_->enableTracking.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Enable Tracking", &enableTracking)) {
        sharedConfig_->enableTracking.store(enableTracking, std::memory_order_relaxed);
        LOG_INFO("Tracking {}", enableTracking ? "enabled" : "disabled");
    }

    // Prediction toggle
    bool enablePrediction = sharedConfig_->enablePrediction.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Enable Prediction", &enablePrediction)) {
        sharedConfig_->enablePrediction.store(enablePrediction, std::memory_order_relaxed);
        LOG_INFO("Prediction {}", enablePrediction ? "enabled" : "disabled");
    }

    // Tremor toggle (Phase 4 humanization)
    bool enableTremor = sharedConfig_->enableTremor.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Enable Tremor (Humanization)", &enableTremor)) {
        sharedConfig_->enableTremor.store(enableTremor, std::memory_order_relaxed);
        LOG_INFO("Tremor {}", enableTremor ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Adds natural hand tremor (8-12 Hz, 0.5px amplitude)");
    }
}

void ConfigApp::renderTelemetryDashboard() {
    ImGui::Text("Telemetry Dashboard");
    ImGui::Separator();

    if (!sharedConfig_) return;

    // FPS
    float captureFPS = sharedConfig_->captureFPS.load(std::memory_order_relaxed);
    ImGui::Text("Capture FPS: %.1f", captureFPS);

    // Latency metrics
    float captureLatency = sharedConfig_->captureLatency.load(std::memory_order_relaxed);
    float detectionLatency = sharedConfig_->detectionLatency.load(std::memory_order_relaxed);
    float trackingLatency = sharedConfig_->trackingLatency.load(std::memory_order_relaxed);
    float inputLatency = sharedConfig_->inputLatency.load(std::memory_order_relaxed);

    ImGui::Text("Latency Breakdown:");
    ImGui::Indent();
    ImGui::Text("  Capture:   %.2f ms", captureLatency);
    ImGui::Text("  Detection: %.2f ms", detectionLatency);
    ImGui::Text("  Tracking:  %.2f ms", trackingLatency);
    ImGui::Text("  Input:     %.2f ms", inputLatency);
    ImGui::Unindent();

    // Active targets
    int activeTargets = sharedConfig_->activeTargets.load(std::memory_order_relaxed);
    ImGui::Text("Active Targets: %d", activeTargets);

    // VRAM usage
    size_t vramUsageMB = sharedConfig_->vramUsageMB.load(std::memory_order_relaxed);
    ImGui::Text("VRAM Usage: %zu MB / 512 MB", vramUsageMB);
    float vramPercent = static_cast<float>(vramUsageMB) / 512.0f;
    ImVec4 vramColor = vramPercent < 0.7f ? ImVec4(0, 1, 0, 1) :
                       vramPercent < 0.9f ? ImVec4(1, 1, 0, 1) :
                       ImVec4(1, 0, 0, 1);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, vramColor);
    ImGui::ProgressBar(vramPercent, ImVec2(-1, 0));
    ImGui::PopStyleColor();
}

void ConfigApp::renderSafetyMetrics() {
    ImGui::Text("Safety Metrics (Critical Traps)");
    ImGui::Separator();

    if (!sharedConfig_) return;

    // Trap 1: Texture Pool Starvation
    uint64_t texturePoolStarved = sharedConfig_->texturePoolStarved.load(std::memory_order_relaxed);
    ImVec4 trap1Color = texturePoolStarved == 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(trap1Color, "Trap 1 (Pool Starved): %llu", texturePoolStarved);
    if (texturePoolStarved > 0) {
        ImGui::TextWrapped("WARNING: Texture pool starvation detected! Check RAII deleter.");
    }

    // Trap 2: Stale Prediction Events
    uint64_t stalePredictionEvents = sharedConfig_->stalePredictionEvents.load(std::memory_order_relaxed);
    ImVec4 trap2Color = stalePredictionEvents < 10 ? ImVec4(0, 1, 0, 1) :
                        stalePredictionEvents < 100 ? ImVec4(1, 1, 0, 1) :
                        ImVec4(1, 0, 0, 1);
    ImGui::TextColored(trap2Color, "Trap 2 (Stale Predictions): %llu", stalePredictionEvents);
    if (stalePredictionEvents > 10) {
        ImGui::TextWrapped("WARNING: Frequent stale predictions (>50ms). Detection thread degraded.");
    }

    // Trap 4: Deadman Switch Triggers
    uint64_t deadmanSwitchTriggered = sharedConfig_->deadmanSwitchTriggered.load(std::memory_order_relaxed);
    ImVec4 trap4Color = deadmanSwitchTriggered == 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(trap4Color, "Trap 4 (Deadman Switch): %llu", deadmanSwitchTriggered);
    if (deadmanSwitchTriggered > 0) {
        ImGui::TextWrapped("WARNING: Deadman switch triggered! Stale commands (>200ms).");
    }
}

bool ConfigApp::createDeviceD3D(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain_,
        &d3dDevice_,
        &featureLevel,
        &d3dDeviceContext_
    );

    if (res != S_OK) {
        LOG_ERROR("D3D11CreateDeviceAndSwapChain failed");
        return false;
    }

    createRenderTarget();
    return true;
}

void ConfigApp::cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (d3dDeviceContext_) { d3dDeviceContext_->Release(); d3dDeviceContext_ = nullptr; }
    if (d3dDevice_) { d3dDevice_->Release(); d3dDevice_ = nullptr; }
}

void ConfigApp::createRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        d3dDevice_->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView_);
        pBackBuffer->Release();
    }
}

void ConfigApp::cleanupRenderTarget() {
    if (mainRenderTargetView_) {
        mainRenderTargetView_->Release();
        mainRenderTargetView_ = nullptr;
    }
}

} // namespace macroman

//
// Main entry point
//
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    macroman::ConfigApp app;

    if (!app.initialize()) {
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
