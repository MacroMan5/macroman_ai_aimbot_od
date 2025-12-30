#include "DuplicationCapture.h"
#include "core/utils/Logger.h"
#include <chrono>

namespace macroman {

DuplicationCapture::~DuplicationCapture() {
    shutdown();
}

bool DuplicationCapture::initialize(void* targetWindowHandle) {
    targetWindow_ = static_cast<HWND>(targetWindowHandle);

    if (!targetWindow_ || !IsWindow(targetWindow_)) {
        lastError_ = "Invalid window handle";
        LOG_ERROR("DuplicationCapture::initialize - {}", lastError_);
        return false;
    }

    // Initialize D3D11 device
    if (!initializeD3D()) {
        return false;
    }

    // Initialize DXGI duplication
    if (!initializeDuplication()) {
        return false;
    }

    // Create texture pool (640x640 for YOLO input)
    texturePool_ = std::make_unique<TexturePool>();
    if (!texturePool_->initialize(d3dDevice_.Get(), 640, 640)) {
        lastError_ = "Failed to initialize texture pool";
        LOG_ERROR("DuplicationCapture::initialize - {}", lastError_);
        return false;
    }

    LOG_INFO("DuplicationCapture initialized successfully");
    return true;
}

bool DuplicationCapture::initializeD3D() {
    D3D_FEATURE_LEVEL featureLevel;
    UINT flags = 0;

#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDevice(
        nullptr,                        // Adapter (use default)
        D3D_DRIVER_TYPE_HARDWARE,       // Hardware acceleration
        nullptr,                        // Software module
        flags,                          // Flags
        nullptr,                        // Feature levels
        0,                              // Num feature levels
        D3D11_SDK_VERSION,              // SDK version
        d3dDevice_.GetAddressOf(),      // Device
        &featureLevel,                  // Feature level
        d3dContext_.GetAddressOf()      // Context
    );

    if (FAILED(hr)) {
        lastError_ = "Failed to create D3D11 device: HRESULT=" + std::to_string(hr);
        LOG_ERROR("DuplicationCapture::initializeD3D - {}", lastError_);
        return false;
    }

    LOG_DEBUG("D3D11 device created (feature level: {:X})", static_cast<uint32_t>(featureLevel));
    return true;
}

bool DuplicationCapture::initializeDuplication() {
    // Get DXGI device
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = d3dDevice_.As(&dxgiDevice);
    if (FAILED(hr)) {
        lastError_ = "Failed to query IDXGIDevice";
        LOG_ERROR("DuplicationCapture::initializeDuplication - {}", lastError_);
        return false;
    }

    // Get adapter
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        lastError_ = "Failed to get DXGI adapter";
        LOG_ERROR("DuplicationCapture::initializeDuplication - {}", lastError_);
        return false;
    }

    // Get primary output (monitor)
    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(0, output.GetAddressOf());
    if (FAILED(hr)) {
        lastError_ = "Failed to enumerate outputs";
        LOG_ERROR("DuplicationCapture::initializeDuplication - {}", lastError_);
        return false;
    }

    // Get IDXGIOutput1 interface
    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        lastError_ = "Failed to query IDXGIOutput1 (requires Windows 8+)";
        LOG_ERROR("DuplicationCapture::initializeDuplication - {}", lastError_);
        return false;
    }

    // Create duplication
    hr = output1->DuplicateOutput(d3dDevice_.Get(), duplication_.GetAddressOf());
    if (FAILED(hr)) {
        lastError_ = "Failed to create desktop duplication: HRESULT=" + std::to_string(hr);
        LOG_ERROR("DuplicationCapture::initializeDuplication - {} (requires non-remote session)", lastError_);
        return false;
    }

    LOG_DEBUG("Desktop duplication initialized");
    return true;
}

void DuplicationCapture::shutdown() {
    if (duplication_) {
        duplication_->ReleaseFrame();  // Release any pending frame
        duplication_.Reset();
    }

    texturePool_.reset();
    d3dContext_.Reset();
    d3dDevice_.Reset();

    LOG_INFO("DuplicationCapture shut down");
}

bool DuplicationCapture::updateWindowRect() {
    if (!GetWindowRect(targetWindow_, &windowRect_)) {
        lastError_ = "Failed to get window rect";
        return false;
    }
    return true;
}

Frame DuplicationCapture::captureFrame() {
    Frame frame;

    // Update window position (in case it moved)
    if (!updateWindowRect()) {
        return frame;  // Invalid frame
    }

    // Acquire next desktop frame
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = duplication_->AcquireNextFrame(
        0,  // Timeout: 0ms (non-blocking)
        &frameInfo,
        desktopResource.GetAddressOf()
    );

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // No new frame available (desktop unchanged)
        // This is not an error, just return invalid frame
        return frame;
    }

    if (hr == DXGI_ERROR_ACCESS_LOST) {
        // Desktop changed (mode switch, driver update)
        LOG_WARN("DuplicationCapture - Access lost, reinitializing...");
        duplication_.Reset();
        if (initializeDuplication()) {
            return captureFrame();  // Retry
        } else {
            lastError_ = "Failed to reinitialize after access lost";
            return frame;
        }
    }

    if (FAILED(hr)) {
        lastError_ = "AcquireNextFrame failed: HRESULT=" + std::to_string(hr);
        LOG_ERROR("DuplicationCapture::captureFrame - {}", lastError_);
        return frame;
    }

    // Get desktop texture
    ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        duplication_->ReleaseFrame();
        lastError_ = "Failed to query ID3D11Texture2D";
        return frame;
    }

    // Acquire texture from pool
    TextureHandle poolTexture = texturePool_->acquireForWrite(++frameSequence_);
    if (!poolTexture) {
        // Pool starved (all textures busy)
        duplication_->ReleaseFrame();
        LOG_WARN("DuplicationCapture - TexturePool starved, dropping frame {}", frameSequence_);
        return frame;  // Drop frame (head-drop policy)
    }

    // Crop and copy desktop texture to pool texture
    if (!cropAndCopy(desktopTexture.Get(), poolTexture->d3dTexture.Get())) {
        duplication_->ReleaseFrame();
        return frame;
    }

    // Release desktop frame
    duplication_->ReleaseFrame();

    // Build Frame struct
    frame.texture = std::move(poolTexture);
    frame.frameSequence = frameSequence_;
    frame.captureTimeNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    frame.width = 640;
    frame.height = 640;

    return frame;
}

bool DuplicationCapture::cropAndCopy(ID3D11Texture2D* desktopTexture, ID3D11Texture2D* targetTexture) {
    // Get desktop texture description
    D3D11_TEXTURE2D_DESC desktopDesc;
    desktopTexture->GetDesc(&desktopDesc);

    // Calculate crop region (centered on window)
    int windowWidth = windowRect_.right - windowRect_.left;
    int windowHeight = windowRect_.bottom - windowRect_.top;

    // Use smaller dimension to create square crop
    int cropSize = (std::min)(windowWidth, windowHeight);

    // Center crop in window
    int cropX = windowRect_.left + (windowWidth - cropSize) / 2;
    int cropY = windowRect_.top + (windowHeight - cropSize) / 2;

    // Clamp to desktop bounds
    cropX = (std::max)(0, (std::min)(cropX, static_cast<int>(desktopDesc.Width) - cropSize));
    cropY = (std::max)(0, (std::min)(cropY, static_cast<int>(desktopDesc.Height) - cropSize));

    // Define source region
    D3D11_BOX srcBox;
    srcBox.left = cropX;
    srcBox.right = cropX + cropSize;
    srcBox.top = cropY;
    srcBox.bottom = cropY + cropSize;
    srcBox.front = 0;
    srcBox.back = 1;

    // Copy subresource region directly (GPU-to-GPU, no staging)
    d3dContext_->CopySubresourceRegion(
        targetTexture,         // Destination resource
        0,                     // Destination subresource
        0, 0, 0,              // Destination X, Y, Z
        desktopTexture,        // Source resource
        0,                     // Source subresource
        &srcBox                // Source box (crop region)
    );

    return true;
}

} // namespace macroman
