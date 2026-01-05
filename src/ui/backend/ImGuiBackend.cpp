#include "ImGuiBackend.h"
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <iostream>

namespace macroman {

ImGuiBackend::ImGuiBackend() = default;

ImGuiBackend::~ImGuiBackend() {
    shutdown();
}

bool ImGuiBackend::initialize(HWND hwnd, D3D11Backend& d3d) {
    std::cout << "[ImGuiBackend] Initializing..." << std::endl;

    if (!d3d.isInitialized()) {
        std::cerr << "[ImGuiBackend] D3D11Backend is not initialized" << std::endl;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // Disable imgui.ini

    setDarkTheme();

    // Note: You must ensure imgui_impl_win32 and imgui_impl_dx11 are available in your build
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(d3d.getDevice(), d3d.getContext());

    initialized_ = true;
    return true;
}

void ImGuiBackend::shutdown() {
    if (initialized_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }
}

void ImGuiBackend::beginFrame() {
    if (!initialized_) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::endFrame() {
    if (!initialized_) return;
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiBackend::setDarkTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
}

void ImGuiBackend::setLightTheme() {
    ImGui::StyleColorsLight();
}

void ImGuiBackend::setTheme() {
    // P10-01: Cyberpunk/Dark theme with custom colors
    // Palette: Dark Grey, Neon Green (success), Red (warning/danger)

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base colors - Dark Grey palette
    const ImVec4 BG_DARK = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);       // Main background
    const ImVec4 BG_MEDIUM = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);     // Window background
    const ImVec4 BG_LIGHT = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);      // Widget background
    const ImVec4 BG_HOVER = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);      // Hover state

    // Accent colors - Cyberpunk Neon
    const ImVec4 ACCENT_GREEN = ImVec4(0.00f, 1.00f, 0.50f, 1.00f);  // Neon green (success)
    const ImVec4 ACCENT_RED = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);    // Danger red
    const ImVec4 ACCENT_ORANGE = ImVec4(1.00f, 0.60f, 0.00f, 1.00f); // Warning orange

    // Text colors
    const ImVec4 TEXT_PRIMARY = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    const ImVec4 TEXT_SECONDARY = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    const ImVec4 TEXT_DISABLED = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    // Window styling
    colors[ImGuiCol_WindowBg] = BG_MEDIUM;
    colors[ImGuiCol_ChildBg] = BG_DARK;
    colors[ImGuiCol_PopupBg] = BG_MEDIUM;
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frame (widget backgrounds)
    colors[ImGuiCol_FrameBg] = BG_LIGHT;
    colors[ImGuiCol_FrameBgHovered] = BG_HOVER;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);

    // Title bar
    colors[ImGuiCol_TitleBg] = BG_DARK;
    colors[ImGuiCol_TitleBgActive] = BG_MEDIUM;
    colors[ImGuiCol_TitleBgCollapsed] = BG_DARK;

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = BG_DARK;
    colors[ImGuiCol_ScrollbarGrab] = BG_LIGHT;
    colors[ImGuiCol_ScrollbarGrabHovered] = BG_HOVER;
    colors[ImGuiCol_ScrollbarGrabActive] = ACCENT_GREEN;

    // Checkmarks, sliders (use neon green)
    colors[ImGuiCol_CheckMark] = ACCENT_GREEN;
    colors[ImGuiCol_SliderGrab] = ACCENT_GREEN;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.80f, 0.40f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = BG_LIGHT;
    colors[ImGuiCol_ButtonHovered] = BG_HOVER;
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);

    // Headers (collapsing headers, tree nodes)
    colors[ImGuiCol_Header] = BG_LIGHT;
    colors[ImGuiCol_HeaderHovered] = BG_HOVER;
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);

    // Separators
    colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ACCENT_GREEN;
    colors[ImGuiCol_SeparatorActive] = ACCENT_GREEN;

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = BG_LIGHT;
    colors[ImGuiCol_ResizeGripHovered] = ACCENT_GREEN;
    colors[ImGuiCol_ResizeGripActive] = ACCENT_GREEN;

    // Tabs
    colors[ImGuiCol_Tab] = BG_DARK;
    colors[ImGuiCol_TabHovered] = BG_HOVER;
    colors[ImGuiCol_TabActive] = BG_MEDIUM;
    colors[ImGuiCol_TabUnfocused] = BG_DARK;
    colors[ImGuiCol_TabUnfocusedActive] = BG_LIGHT;

    // Text
    colors[ImGuiCol_Text] = TEXT_PRIMARY;
    colors[ImGuiCol_TextDisabled] = TEXT_DISABLED;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.50f, 0.25f, 0.50f);

    // Plot colors (use green/red for graphs)
    colors[ImGuiCol_PlotLines] = ACCENT_GREEN;
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 0.70f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ACCENT_ORANGE;
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.70f, 0.00f, 1.00f);

    // Rounding (smooth edges for cyberpunk feel)
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;

    // Padding and spacing
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);

    // Borders
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // Alpha
    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.5f;

    std::cout << "[ImGuiBackend] Cyberpunk theme applied (Dark Grey + Neon Green)" << std::endl;
}

} // namespace macroman
