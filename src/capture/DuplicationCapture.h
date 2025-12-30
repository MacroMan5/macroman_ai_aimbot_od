#pragma once

#include "../../core/interfaces/IScreenCapture.h"
#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace sunone {

class DuplicationCapture : public IScreenCapture {
public:
    DuplicationCapture();
    ~DuplicationCapture() override;

    // IScreenCapture
    bool initialize() override;
    void release() override;
    Frame getNextFrame() override;
    Frame getGpuFrame() override;

    std::string getName() const override { return "Desktop Duplication"; }
    bool isAvailable() const override;

    void setRegion(int x, int y, int width, int height) override;
    void setTargetFps(int fps) override;
    void setCpuReadback(bool enabled) override { cpuReadback_ = enabled; }

    // Specific
    void setMonitor(int index);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    int regionX_ = 0;
    int regionY_ = 0;
    int regionWidth_ = 640;
    int regionHeight_ = 640;
    int targetFps_ = 60;
    int monitorIndex_ = 0;
    uint64_t frameCounter_ = 0;
    bool initialized_ = false;
    bool cpuReadback_ = false; // Default to false for zero-copy performance
};

} // namespace sunone
