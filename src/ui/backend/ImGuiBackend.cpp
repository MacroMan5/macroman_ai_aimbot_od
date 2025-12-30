#include "ImGuiBackend.h"
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <iostream>

namespace sunone {

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

} // namespace sunone
