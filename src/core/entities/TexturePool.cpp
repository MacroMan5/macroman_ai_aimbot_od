#include "TexturePool.h"
#include "utils/Logger.h"
#include <algorithm>

namespace macroman {

TexturePool::~TexturePool() {
    shutdown();
}

bool TexturePool::initialize(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (!device) {
        LOG_ERROR("TexturePool::initialize - device is null");
        return false;
    }

    d3dDevice_ = device;

    // Create D3D11 textures
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // Standard BGRA format
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;  // GPU-only (zero-copy)
    desc.MiscFlags = 0;

    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = d3dDevice_->CreateTexture2D(&desc, nullptr, &texture);

        if (FAILED(hr)) {
            LOG_ERROR("TexturePool::initialize - Failed to create texture {}: HRESULT={:X}", i, static_cast<uint32_t>(hr));
            shutdown();
            return false;
        }

        pool_[i].d3dTexture.Attach(texture);
        pool_[i].refCount.store(0, std::memory_order_relaxed);
        pool_[i].width = width;
        pool_[i].height = height;
    }

    LOG_INFO("TexturePool initialized: {} textures, {}x{}", POOL_SIZE, width, height);
    return true;
}

TextureHandle TexturePool::acquireForWrite(uint64_t frameId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find texture with refCount == 0
    for (auto& tex : pool_) {
        int expected = 0;
        if (tex.refCount.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            // Acquired successfully
            tex.frameId = frameId;
            return TextureHandle(&tex, TextureDeleter{this});
        }
    }

    // All textures busy (pool starved)
    starvedCount_.fetch_add(1, std::memory_order_relaxed);
    LOG_WARN("TexturePool starved at frame {} (all {} textures busy)", frameId, POOL_SIZE);
    return nullptr;
}

void TexturePool::release(Texture* tex) noexcept {
    if (!tex) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Verify texture belongs to this pool
    bool found = false;
    for (auto& poolTex : pool_) {
        if (&poolTex == tex) {
            found = true;
            break;
        }
    }

    if (!found) {
        LOG_ERROR("TexturePool::release - texture does not belong to this pool");
        return;
    }

    // Release texture
    int prevCount = tex->refCount.fetch_sub(1, std::memory_order_release);
    if (prevCount <= 0) {
        LOG_ERROR("TexturePool::release - refCount was already 0 (double-release?)");
    }
}

size_t TexturePool::getAvailableCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count_if(pool_.begin(), pool_.end(), [](const Texture& tex) {
        return tex.refCount.load(std::memory_order_relaxed) == 0;
    });
}

void TexturePool::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& tex : pool_) {
        tex.d3dTexture.Reset();
        tex.refCount.store(0, std::memory_order_relaxed);
    }

    d3dDevice_ = nullptr;
    LOG_INFO("TexturePool shut down");
}

} // namespace macroman
