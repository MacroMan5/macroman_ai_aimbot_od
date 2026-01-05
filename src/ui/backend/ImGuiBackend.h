#pragma once

#include "D3D11Backend.h"
#include <imgui.h>

namespace macroman {

class ImGuiBackend {
public:
    ImGuiBackend();
    ~ImGuiBackend();

    bool initialize(HWND hwnd, D3D11Backend& d3d);
    void shutdown();

    void beginFrame();
    void endFrame();

    // Style
    void setDarkTheme();
    void setLightTheme();
    void setTheme();  // P10-01: Cyberpunk theme with custom colors

    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace macroman
