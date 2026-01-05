#pragma once

#include "../entities/Frame.h"
#include <string>

namespace macroman {

/**
 * @brief Screen capture abstraction
 *
 * Implementations:
 * - WinrtCapture: Windows.Graphics.Capture (144+ FPS, Windows 10 1903+)
 * - DuplicationCapture: Desktop Duplication API (120+ FPS, Windows 8+)
 *
 * Thread Safety: All methods must be called from the same thread (Capture Thread).
 *
 * Lifecycle:
 * 1. initialize() - Set up capture context + TexturePool
 * 2. captureFrame() - Called repeatedly at target FPS
 * 3. shutdown() - Clean up resources
 */
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;

    /**
     * @brief Initialize capture for target window
     * @param targetWindowHandle HWND of game window (cast to void*)
     * @return true if initialization succeeded
     */
    virtual bool initialize(void* targetWindowHandle) = 0;

    /**
     * @brief Capture single frame (non-blocking)
     * @return Frame with RAII texture handle, or invalid Frame on error
     *
     * Performance Target: <1ms (P95: 2ms)
     *
     * CRITICAL: Frame owns texture via unique_ptr. When Frame is deleted,
     * texture is automatically released back to pool. No manual cleanup needed.
     */
    virtual Frame captureFrame() = 0;

    /**
     * @brief Clean up resources (blocking)
     *
     * Must be called before destructor. Blocks until pending
     * capture operations complete.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get last error message (if any)
     */
    [[nodiscard]] virtual std::string getLastError() const = 0;
};

} // namespace macroman
