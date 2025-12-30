#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "../../core/interfaces/IMouseDriver.h"

namespace sunone {

/**
 * @brief Win32 SendInput mouse driver implementation.
 *
 * Uses the Windows SendInput API for mouse movement and button presses.
 * Always available on Windows systems.
 */
class Win32Driver : public IMouseDriver {
public:
    Win32Driver() = default;
    ~Win32Driver() override = default;

    // Lifecycle
    bool initialize() override;
    void shutdown() override;

    // Core operations
    void move(int dx, int dy) override;
    void press(MouseButton button) override;
    void release(MouseButton button) override;
    void click(MouseButton button) override;

    // Info
    std::string getName() const override { return "Win32 SendInput"; }
    bool isConnected() const override { return true; }

private:
    DWORD buttonToDownFlag(MouseButton button) const;
    DWORD buttonToUpFlag(MouseButton button) const;
};

} // namespace sunone
