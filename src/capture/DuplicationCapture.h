#pragma once

/**
 * SPECIFICATION: DuplicationCapture
 * ==================================
 *
 * PURPOSE:
 * Capture game window using Desktop Duplication API (DXGI 1.2+).
 * Fallback option when WinrtCapture is unavailable (Windows 8+).
 *
 * PERFORMANCE:
 * - Target: 120+ FPS (8ms per frame)
 * - Actual: <1ms when no desktop changes detected
 * - Latency: Lower than WinRT (synchronous API)
 *
 * LIMITATIONS:
 * - Captures entire monitor (not specific window)
 * - Requires cropping to game window region
 * - No capture if game is minimized or occluded
 *
 * ARCHITECTURE:
 * 1. IDXGIOutputDuplication::AcquireNextFrame() - Get latest desktop frame
 * 2. Crop to game window region (using HWND position)
 * 3. Copy to TexturePool texture via ID3D11DeviceContext::CopyResource()
 * 4. IDXGIOutputDuplication::ReleaseFrame()
 *
 * THREADING:
 * Single-threaded (Capture Thread only). DXGI duplication is not thread-safe.
 *
 * ERROR HANDLING:
 * - DXGI_ERROR_ACCESS_LOST: Desktop changed (mode switch, driver update)
 *   → Reinitialize duplication
 * - DXGI_ERROR_WAIT_TIMEOUT: No desktop changes in timeout period
 *   → Return last frame (or empty frame)
 */

#include "core/interfaces/IScreenCapture.h"
#include "core/entities/TexturePool.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <memory>

namespace macroman {

using Microsoft::WRL::ComPtr;

class DuplicationCapture : public IScreenCapture {
private:
    // D3D11 resources
    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<IDXGIOutputDuplication> duplication_;

    // Texture pool
    std::unique_ptr<TexturePool> texturePool_;

    // Target window
    HWND targetWindow_{nullptr};
    RECT windowRect_{};  // Cached window position (updated per frame)

    // Frame sequencing
    uint64_t frameSequence_{0};

    // Error tracking
    std::string lastError_;

    // Helper methods
    bool initializeD3D();
    bool initializeDuplication();
    bool updateWindowRect();
    bool cropAndCopy(ID3D11Texture2D* desktopTexture, ID3D11Texture2D* targetTexture);

public:
    DuplicationCapture() = default;
    ~DuplicationCapture() override;

    // IScreenCapture interface
    bool initialize(void* targetWindowHandle) override;
    Frame captureFrame() override;
    void shutdown() override;
    [[nodiscard]] std::string getLastError() const override { return lastError_; }
};

} // namespace macroman
