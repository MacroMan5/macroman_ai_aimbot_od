#pragma once

#include "../types/Enums.h"
#include <string>

namespace macroman {

/**
 * @brief Mouse input abstraction for sending aim commands
 *
 * Implementations:
 * - WindowsInputDriver: SendInput API (sub-millisecond latency, Windows native)
 * - ArduinoDriver: Serial-based HID emulation (hardware-level, anti-cheat resistant)
 *
 * @note Thread Safety: All methods must be called from the same thread (Input Thread).
 * The Input thread owns this object and calls move() at 1000 Hz.
 *
 * Lifecycle:
 * 1. initialize() - Set up mouse driver connection
 * 2. move() - Repeatedly send relative movements (1000 Hz loop)
 * 3. shutdown() - Clean up driver resources
 */
class IMouseDriver {
public:
    virtual ~IMouseDriver() = default;

    /**
     * @brief Initialize mouse driver
     * @return true if driver is ready to send inputs
     *
     * @note Thread Safety: Must be called from Input thread
     * @note Performance: Typically <10ms initialization time
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown driver and release resources
     *
     * @note Thread Safety: Must be called from Input thread
     * @note Blocks until pending operations complete
     */
    virtual void shutdown() = 0;

    /**
     * @brief Send relative mouse movement
     * @param dx Horizontal delta in pixels (positive = right)
     * @param dy Vertical delta in pixels (positive = down)
     *
     * @note Performance Target: <0.5ms (P95: 1ms)
     * @note Thread Safety: Called by Input thread at 1000 Hz
     */
    virtual void move(int dx, int dy) = 0;

    /**
     * @brief Send absolute mouse position (optional)
     * @param x Absolute screen X coordinate
     * @param y Absolute screen Y coordinate
     *
     * @note Default implementation does nothing (check supportsAbsoluteMovement())
     * @note Thread Safety: Called by Input thread only
     */
    virtual void moveAbsolute(int x, int y) { /* Optional */ }

    /**
     * @brief Press mouse button (down state)
     * @param button Button to press (Left, Right, Middle)
     *
     * @note Thread Safety: Called by Input thread only
     */
    virtual void press(MouseButton button) = 0;

    /**
     * @brief Release mouse button (up state)
     * @param button Button to release
     *
     * @note Thread Safety: Called by Input thread only
     */
    virtual void release(MouseButton button) = 0;

    /**
     * @brief Send mouse click (press + release)
     * @param button Button to click
     *
     * @note Thread Safety: Called by Input thread only
     */
    virtual void click(MouseButton button) = 0;

    /**
     * @brief Get driver implementation name
     * @return "Windows SendInput", "Arduino HID", etc.
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Check if driver is ready to send inputs
     * @return true if initialize() succeeded and driver is connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Check if driver supports absolute positioning
     * @return true if moveAbsolute() is implemented
     */
    virtual bool supportsAbsoluteMovement() const { return false; }

    /**
     * @brief Check if driver supports sub-pixel precision
     * @return true if driver uses high-resolution movement API
     */
    virtual bool supportsHighPrecision() const { return false; }
};

} // namespace macroman
