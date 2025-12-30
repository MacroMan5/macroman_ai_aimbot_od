#pragma once

#include "../core/interfaces/IScreenCapture.h"
#include <memory>
#include <string>

namespace sunone {

class WinrtCapture : public IScreenCapture {
public:
    WinrtCapture();
    ~WinrtCapture() override;

    // IScreenCapture
    bool initialize() override;
    void release() override;
    Frame getNextFrame() override;

    std::string getName() const override { return "WinRT Graphics Capture"; }
    bool isAvailable() const override;

    void setRegion(int x, int y, int width, int height) override;
    void setTargetFps(int fps) override;
    void setCpuReadback(bool enabled) override { cpuReadback_ = enabled; }

    // Specific
    void setTargetWindow(const std::string& windowTitle);
    void setCaptureEntireScreen(bool entire);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    std::string targetWindow_;
    bool captureEntireScreen_ = true;
    bool cpuReadback_ = false; // Default to false
    int regionX_ = 0;
    int regionY_ = 0;
    int regionWidth_ = 640;
    int regionHeight_ = 640;
    int targetFps_ = 60;
    uint64_t frameCounter_ = 0;
    bool initialized_ = false;
};

} // namespace sunone
