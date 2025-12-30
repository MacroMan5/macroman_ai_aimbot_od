#pragma once

#include "../types/Enums.h"
#include <string>

namespace sunone {

class IMouseDriver {
public:
    virtual ~IMouseDriver() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // Core operations
    virtual void move(int dx, int dy) = 0;
    virtual void moveAbsolute(int x, int y) { /* Optional */ }
    virtual void press(MouseButton button) = 0;
    virtual void release(MouseButton button) = 0;
    virtual void click(MouseButton button) = 0;

    // Info
    virtual std::string getName() const = 0;
    virtual bool isConnected() const = 0;

    // Capabilities
    virtual bool supportsAbsoluteMovement() const { return false; }
    virtual bool supportsHighPrecision() const { return false; }
};

} // namespace sunone
