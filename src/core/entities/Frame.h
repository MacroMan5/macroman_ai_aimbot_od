#pragma once

#include "Texture.h"
#include <cstdint>

namespace macroman {

/**
 * @brief Frame data captured from screen
 *
 * CRITICAL: Texture lifetime managed by TexturePool via RAII wrapper.
 * When Frame is deleted (e.g., by LatestFrameQueue head-drop), the
 * TextureDeleter automatically releases the texture back to the pool.
 *
 * This design prevents the "Leak on Drop" trap (Critical Trap #1):
 * - Old frames dropped by LatestFrameQueue trigger Frame destructor
 * - Frame destructor triggers TextureHandle (unique_ptr) destructor
 * - TextureHandle destructor calls TextureDeleter::operator()
 * - TextureDeleter calls pool->release(texture), decrementing refCount
 * - Texture becomes available for next capture
 */
struct Frame {
    TextureHandle texture{nullptr};             // RAII handle (manages pool lifetime)
    uint64_t frameSequence{0};                  // Monotonic sequence number
    int64_t captureTimeNs{0};                   // Capture timestamp (nanoseconds since epoch)
    uint32_t width{0};                          // Texture width in pixels
    uint32_t height{0};                         // Texture height in pixels

    // Default constructor
    Frame() = default;

    // Move constructor and assignment (TextureHandle is move-only)
    Frame(Frame&&) = default;
    Frame& operator=(Frame&&) = default;

    // Deleted copy constructor and assignment (TextureHandle is not copyable)
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    /**
     * @brief Check if frame contains valid data
     * @return true if texture is valid and dimensions are non-zero
     */
    [[nodiscard]] bool isValid() const noexcept {
        return texture != nullptr && texture->isValid() && width > 0 && height > 0;
    }

    /**
     * @brief Get D3D11 texture pointer for detection thread
     * @return D3D11 texture pointer, or nullptr if frame is invalid
     */
    [[nodiscard]] ID3D11Texture2D* getD3DTexture() const noexcept {
        return texture ? texture->d3dTexture.Get() : nullptr;
    }
};

} // namespace macroman
