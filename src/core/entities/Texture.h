#pragma once

/**
 * SPECIFICATION: Texture + TexturePool
 * ====================================
 *
 * CRITICAL PROBLEM ("Leak on Drop" Trap):
 * LatestFrameQueue drops old frames. If Frame holds a raw Texture*, dropping
 * the frame WITHOUT notifying TexturePool will STARVE the pool (all textures
 * marked as 'busy') within 3 frames at 144 FPS.
 *
 * SOLUTION: RAII Wrapper with Custom Deleter
 * Frame holds std::unique_ptr<Texture, TextureDeleter> instead of raw pointer.
 * When Frame is deleted, TextureDeleter::operator() automatically releases
 * the texture back to the pool.
 *
 * MEMORY LAYOUT:
 * - ID3D11Texture2D* (8 bytes, GPU resource pointer)
 * - std::atomic<int> refCount (4 bytes, for diagnostics)
 * - uint64_t frameId (8 bytes, for debugging)
 * Total: 24 bytes per texture
 *
 * TRIPLE BUFFER RATIONALE:
 * - Texture 0: Being written by Capture Thread
 * - Texture 1: Being read by Detection Thread
 * - Texture 2: Available for next capture
 *
 * If all 3 textures are busy, Capture Thread DROPS the frame (head-drop policy).
 *
 * VALIDATION ASSERTIONS:
 * - ASSERT(pool.getAvailableCount() > 0) in Capture Thread
 * - Track metric: texturePoolStarved (should always be 0)
 */

#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdint>

namespace macroman {

// Forward declaration
class TexturePool;

/**
 * @brief GPU texture with lifetime managed by TexturePool
 */
struct Texture {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dTexture{nullptr};
    std::atomic<int> refCount{0};  // For diagnostics only (pool manages lifetime)
    uint64_t frameId{0};           // Frame sequence number (for debugging)
    uint32_t width{0};
    uint32_t height{0};

    [[nodiscard]] bool isValid() const noexcept {
        return d3dTexture != nullptr && width > 0 && height > 0;
    }
};

/**
 * @brief Custom deleter for std::unique_ptr<Texture>
 *
 * When unique_ptr goes out of scope, this operator() is called,
 * which releases the texture back to the pool.
 */
struct TextureDeleter {
    TexturePool* pool{nullptr};

    void operator()(Texture* tex) const noexcept;
};

/**
 * @brief RAII handle for Texture
 *
 * Usage in Frame:
 *   struct Frame {
 *       TextureHandle texture;  // Automatically releases on destruction
 *   };
 */
using TextureHandle = std::unique_ptr<Texture, TextureDeleter>;

} // namespace macroman
