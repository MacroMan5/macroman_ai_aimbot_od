#include "DebugOverlay.h"
#include "../../core/utils/Logger.h"
#include <format>

namespace macroman {

DebugOverlay::DebugOverlay() = default;

DebugOverlay::~DebugOverlay() {
    shutdown();
}

bool DebugOverlay::initialize(HWND hwnd, int screenWidth, int screenHeight) {
    if (initialized_) {
        LOG_WARN("DebugOverlay already initialized");
        return true;
    }

    overlayWindow_ = hwnd;
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    // Enable screenshot protection (Phase 6 requirement)
    enableScreenshotProtection();

    initialized_ = true;
    LOG_INFO("DebugOverlay initialized ({}x{}, screenshot protection enabled)",
             screenWidth, screenHeight);

    return true;
}

void DebugOverlay::shutdown() {
    if (!initialized_) {
        return;
    }

    overlayWindow_ = nullptr;
    initialized_ = false;

    LOG_INFO("DebugOverlay shutdown");
}

void DebugOverlay::enableScreenshotProtection() {
    if (!overlayWindow_) {
        LOG_ERROR("Cannot enable screenshot protection: no window handle");
        return;
    }

    // SetWindowDisplayAffinity makes the overlay invisible to:
    // - OBS recordings
    // - Discord screen share
    // - Anti-cheat screenshots
    // - Windows.Graphics.Capture API
    //
    // Requires Windows 10 1903+
    if (!SetWindowDisplayAffinity(overlayWindow_, WDA_EXCLUDEFROMCAPTURE)) {
        DWORD error = GetLastError();
        LOG_WARN("Failed to enable screenshot protection (error {}). "
                 "Overlay may be visible in screenshots. "
                 "Requires Windows 10 1903+",
                 error);
    } else {
        LOG_INFO("Screenshot protection enabled (WDA_EXCLUDEFROMCAPTURE)");
    }
}

void DebugOverlay::render(const TelemetrySnapshot& telemetry,
                          const TargetSnapshot& targets,
                          SharedConfig* sharedConfig) {
    if (!initialized_ || !config_.enabled) {
        return;
    }

    // Update profiler with latest telemetry
    profiler_.update(telemetry.captureLatency,
                    telemetry.detectionLatency,
                    telemetry.trackingLatency,
                    telemetry.inputLatency);

    // Set overlay alpha
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, config_.overlayAlpha);

    // Render panels
    if (config_.showMetrics) {
        renderMetricsPanel(telemetry);
    }

    if (config_.showBBoxes && targets.count > 0) {
        renderBoundingBoxes(targets);
    }

    if (config_.showComponentToggles && sharedConfig) {
        renderComponentToggles(sharedConfig);
    }

    if (config_.showSafetyMetrics) {
        renderSafetyMetrics(telemetry);
    }

    if (config_.showProfiler) {
        renderProfilerPanel(telemetry);
    }

    ImGui::PopStyleVar();
}

void DebugOverlay::renderMetricsPanel(const TelemetrySnapshot& telemetry) {
    ImGui::SetNextWindowPos(config_.position, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);  // Semi-transparent background

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Performance Metrics", nullptr, flags)) {
        // FPS (color-coded)
        ImGui::Text("FPS:");
        ImGui::SameLine();
        ImVec4 fpsColor = telemetry.captureFPS >= 120.0f ? ImVec4(0, 1, 0, 1) :
                         telemetry.captureFPS >= 60.0f ? ImVec4(1, 1, 0, 1) :
                         ImVec4(1, 0, 0, 1);
        ImGui::TextColored(fpsColor, "%.1f", telemetry.captureFPS);

        // End-to-end latency (color-coded)
        ImGui::Text("Latency:");
        ImGui::SameLine();
        ImVec4 latencyColor = getLatencyColor(telemetry.endToEndLatency);
        ImGui::TextColored(latencyColor, "%.2f ms", telemetry.endToEndLatency);

        ImGui::Separator();

        // Per-stage latency breakdown
        ImGui::Text("Breakdown:");
        ImGui::Indent();

        ImGui::TextColored(getLatencyColor(telemetry.captureLatency),
                          "  Capture: %.2f ms", telemetry.captureLatency);

        ImGui::TextColored(getLatencyColor(telemetry.detectionLatency),
                          "  Detection: %.2f ms", telemetry.detectionLatency);

        ImGui::TextColored(getLatencyColor(telemetry.trackingLatency),
                          "  Tracking: %.2f ms", telemetry.trackingLatency);

        ImGui::TextColored(getLatencyColor(telemetry.inputLatency),
                          "  Input: %.2f ms", telemetry.inputLatency);

        ImGui::Unindent();
        ImGui::Separator();

        // Resource usage
        ImGui::Text("Targets: %d", telemetry.activeTargets);

        ImGui::Text("VRAM:");
        ImGui::SameLine();
        float vramPercent = static_cast<float>(telemetry.vramUsageMB) / 512.0f;
        ImVec4 vramColor = vramPercent < 0.7f ? ImVec4(0, 1, 0, 1) :
                          vramPercent < 0.9f ? ImVec4(1, 1, 0, 1) :
                          ImVec4(1, 0, 0, 1);
        ImGui::TextColored(vramColor, "%zu MB / 512 MB", telemetry.vramUsageMB);

        // VRAM usage bar
        ImGui::ProgressBar(vramPercent, ImVec2(-1, 0));

        ImGui::End();
    }
}

void DebugOverlay::renderBoundingBoxes(const TargetSnapshot& targets) {
    auto* drawList = ImGui::GetBackgroundDrawList();

    for (size_t i = 0; i < targets.count; ++i) {
        const auto& bbox = targets.bboxes[i];

        // Primary color: Selected target = GREEN, others by hitbox
        ImU32 color;
        float thickness = 2.0f;

        if (i == targets.selectedTargetIndex) {
            color = IM_COL32(0, 255, 0, 255);  // GREEN for selected
            thickness = 3.0f;  // Thicker border for selected
        } else {
            color = getHitboxColor(targets.hitboxTypes[i]);
        }

        // Draw bounding box
        drawList->AddRect(
            ImVec2(bbox.x, bbox.y),
            ImVec2(bbox.x + bbox.width, bbox.y + bbox.height),
            color,
            0.0f,  // No rounding
            0,     // All corners
            thickness
        );

        // Draw confidence text
        std::string confidenceText = std::format("{:.0f}%", targets.confidences[i] * 100.0f);
        ImVec2 textPos(bbox.x, bbox.y - 20.0f);  // Above bounding box
        drawList->AddText(textPos, color, confidenceText.c_str());
    }
}

void DebugOverlay::renderComponentToggles(SharedConfig* sharedConfig) {
    ImGui::SetNextWindowPos(ImVec2(config_.position.x, config_.position.y + 250.0f),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Component Toggles", nullptr, flags)) {
        ImGui::Text("Runtime Controls:");
        ImGui::Separator();

        // Aiming toggle
        bool enableAiming = sharedConfig->enableAiming.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Enable Aiming", &enableAiming)) {
            sharedConfig->enableAiming.store(enableAiming, std::memory_order_relaxed);
            LOG_INFO("Aiming {}", enableAiming ? "enabled" : "disabled");
        }

        // Tracking toggle
        bool enableTracking = sharedConfig->enableTracking.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Enable Tracking", &enableTracking)) {
            sharedConfig->enableTracking.store(enableTracking, std::memory_order_relaxed);
            LOG_INFO("Tracking {}", enableTracking ? "enabled" : "disabled");
        }

        // Prediction toggle
        bool enablePrediction = sharedConfig->enablePrediction.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Enable Prediction", &enablePrediction)) {
            sharedConfig->enablePrediction.store(enablePrediction, std::memory_order_relaxed);
            LOG_INFO("Prediction {}", enablePrediction ? "enabled" : "disabled");
        }

        // Tremor toggle (Phase 4 humanization)
        bool enableTremor = sharedConfig->enableTremor.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Enable Tremor", &enableTremor)) {
            sharedConfig->enableTremor.store(enableTremor, std::memory_order_relaxed);
            LOG_INFO("Tremor {}", enableTremor ? "enabled" : "disabled");
        }

        ImGui::Separator();

        // Live tuning sliders
        ImGui::Text("Live Tuning:");

        float smoothness = sharedConfig->aimSmoothness.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Smoothness", &smoothness, 0.0f, 1.0f, "%.2f")) {
            sharedConfig->aimSmoothness.store(smoothness, std::memory_order_relaxed);
        }

        float fov = sharedConfig->fov.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("FOV", &fov, 10.0f, 180.0f, "%.0f deg")) {
            sharedConfig->fov.store(fov, std::memory_order_relaxed);
        }

        ImGui::End();
    }
}

void DebugOverlay::renderSafetyMetrics(const TelemetrySnapshot& telemetry) {
    ImGui::SetNextWindowPos(ImVec2(config_.position.x + 300.0f, config_.position.y),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Safety Metrics (Advanced)", nullptr, flags)) {
        ImGui::Text("Critical Traps Monitoring:");
        ImGui::Separator();

        // Trap 1: Texture Pool Starvation
        ImVec4 trap1Color = telemetry.texturePoolStarved == 0 ? ImVec4(0, 1, 0, 1) :
                           ImVec4(1, 0, 0, 1);
        ImGui::TextColored(trap1Color, "Trap 1 (Pool Starved): %llu",
                          telemetry.texturePoolStarved);
        if (telemetry.texturePoolStarved > 0) {
            ImGui::TextWrapped("WARNING: Texture pool starvation detected! "
                              "Check RAII deleter implementation.");
        }

        // Trap 2: Stale Prediction Events
        ImVec4 trap2Color = telemetry.stalePredictionEvents < 10 ? ImVec4(0, 1, 0, 1) :
                           telemetry.stalePredictionEvents < 100 ? ImVec4(1, 1, 0, 1) :
                           ImVec4(1, 0, 0, 1);
        ImGui::TextColored(trap2Color, "Trap 2 (Stale Predictions): %llu",
                          telemetry.stalePredictionEvents);
        if (telemetry.stalePredictionEvents > 10) {
            ImGui::TextWrapped("WARNING: Frequent stale predictions (>50ms). "
                              "Detection thread may be degraded.");
        }

        // Trap 4: Deadman Switch Triggers
        ImVec4 trap4Color = telemetry.deadmanSwitchTriggered == 0 ? ImVec4(0, 1, 0, 1) :
                           ImVec4(1, 0, 0, 1);
        ImGui::TextColored(trap4Color, "Trap 4 (Deadman Switch): %llu",
                          telemetry.deadmanSwitchTriggered);
        if (telemetry.deadmanSwitchTriggered > 0) {
            ImGui::TextWrapped("WARNING: Deadman switch triggered! "
                              "Input thread detected stale commands (>200ms).");
        }

        ImGui::End();
    }
}

void DebugOverlay::renderProfilerPanel(const TelemetrySnapshot& telemetry) {
    ImGui::SetNextWindowBgAlpha(0.85f);  // Slightly more opaque for graphs

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    // Position below metrics panel
    ImVec2 profilerPos = config_.position;
    profilerPos.y += 200.0f;  // Offset below metrics panel
    ImGui::SetNextWindowPos(profilerPos, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Frame Profiler", nullptr, flags)) {
        // Delegate to FrameProfiler for rendering graphs
        profiler_.renderGraphs();

        ImGui::End();
    }
}

ImU32 DebugOverlay::getHitboxColor(HitboxType type) const {
    switch (type) {
        case HitboxType::Head:
            return IM_COL32(255, 0, 0, 255);    // RED
        case HitboxType::Chest:
            return IM_COL32(255, 165, 0, 255);  // ORANGE
        case HitboxType::Body:
            return IM_COL32(255, 255, 0, 255);  // YELLOW
        default:
            return IM_COL32(128, 128, 128, 255); // GRAY
    }
}

ImVec4 DebugOverlay::getLatencyColor(float latencyMs) const {
    if (latencyMs < 5.0f) {
        return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // GREEN (excellent)
    } else if (latencyMs < 10.0f) {
        return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // YELLOW (good)
    } else {
        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // RED (poor)
    }
}

} // namespace macroman
