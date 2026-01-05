#pragma once

#include "Texture.h"
#include <array>
#include <mutex>
#include <memory>

namespace macroman {

/**
 * @brief Triple-buffer texture pool for zero-copy GPU capture
 *
 * Thread Safety: Thread-safe via mutex (not in hot path, only on acquire/release)
 *
 * Lifecycle:
 * 1. initialize() - Create 3 D3D11 textures
 * 2. acquireForWrite() - Called by Capture Thread
 * 3. release() - Called automatically by TextureDeleter
 * 4. shutdown() - Releases all D3D11 resources
 */
class TexturePool {
private:
    static constexpr size_t POOL_SIZE = 3;  // Triple buffer

    std::array<Texture, POOL_SIZE> pool_;
    mutable std::mutex mutex_;  // Mutable so it can be locked in const methods
    ID3D11Device* d3dDevice_{nullptr};  // Not owned (provided by capture system)

    // Metrics
    std::atomic<size_t> starvedCount_{0};  // Times acquireForWrite returned nullptr

public:
    TexturePool() = default;
    ~TexturePool();

    // Non-copyable, non-movable
    TexturePool(const TexturePool&) = delete;
    TexturePool& operator=(const TexturePool&) = delete;

    /**
     * @brief Initialize pool with D3D11 textures
     * @param device D3D11 device (not owned, must outlive pool)
     * @param width Texture width
     * @param height Texture height
     * @return true if initialization succeeded
     */
    bool initialize(ID3D11Device* device, uint32_t width, uint32_t height);

    /**
     * @brief Acquire texture for writing (Capture Thread)
     * @param frameId Frame sequence number (for debugging)
     * @return RAII handle, or nullptr if all textures busy
     *
     * CRITICAL: If returns nullptr, Capture Thread must DROP the frame.
     * This prevents blocking and maintains real-time guarantee.
     */
    [[nodiscard]] TextureHandle acquireForWrite(uint64_t frameId);

    /**
     * @brief Release texture back to pool (called by TextureDeleter)
     * @param tex Texture to release (must be from this pool)
     */
    void release(Texture* tex) noexcept;

    /**
     * @brief Get count of available textures (for debugging)
     */
    [[nodiscard]] size_t getAvailableCount() const noexcept;

    /**
     * @brief Get count of times pool was starved (metric)
     */
    [[nodiscard]] size_t getStarvedCount() const noexcept {
        return starvedCount_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clean up D3D11 resources
     */
    void shutdown();
};

// TextureDeleter implementation (requires TexturePool definition)
inline void TextureDeleter::operator()(Texture* tex) const noexcept {
    if (pool && tex) {
        pool->release(tex);
    }
}

} // namespace macroman
