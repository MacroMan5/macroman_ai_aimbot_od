#pragma once

#include "../entities/Frame.h"
#include <string>

namespace macroman {

class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void release() = 0;

    // Core operation
    virtual Frame getNextFrame() = 0;
    
    // GPU-accelerated path (optional, returns empty frame if not supported)
    virtual Frame getGpuFrame() { return Frame(); }

    // Info
    virtual std::string getName() const = 0;
    virtual bool isAvailable() const = 0;

    // Optional: configuration
    virtual void setRegion(int x, int y, int width, int height) {}
    virtual void setTargetFps(int fps) {}
    virtual void setCpuReadback(bool enabled) {}
};

} // namespace macroman
