// Include OpenCV first to avoid Windows macro conflicts
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include "DuplicationCapture.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>
#include <iostream>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace sunone {

class DuplicationCapture::Impl {
public:
    Impl() {
        // Init D3D
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
        };
        
        // Create device
        HRESULT hr = D3D11CreateDevice(
            nullptr, 
            D3D_DRIVER_TYPE_HARDWARE, 
            nullptr, 
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
            featureLevels, 
            ARRAYSIZE(featureLevels), 
            D3D11_SDK_VERSION, 
            device.GetAddressOf(), 
            nullptr, 
            context.GetAddressOf());

        if (FAILED(hr)) {
            std::cerr << "[DuplicationCapture] Failed to create D3D11 device" << std::endl;
        }
    }

    ~Impl() {
        release();
    }

    bool initialize(int monitorIndex) {
        if (!device) return false;

        ComPtr<IDXGIDevice> dxgiDevice;
        if (FAILED(device.As(&dxgiDevice))) return false;

        ComPtr<IDXGIAdapter> adapter;
        if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) return false;

        ComPtr<IDXGIOutput> output;
        if (FAILED(adapter->EnumOutputs(monitorIndex, output.GetAddressOf()))) {
            std::cerr << "[DuplicationCapture] Monitor index " << monitorIndex << " not found" << std::endl;
            return false;
        }

        ComPtr<IDXGIOutput1> output1;
        if (FAILED(output.As(&output1))) return false;

        // Create duplication
        if (FAILED(output1->DuplicateOutput(device.Get(), duplication.GetAddressOf()))) {
            // This often fails if there is already a duplication session active (e.g. OBS)
            // or if the desktop switch happens (UAC, Ctrl+Alt+Del)
            std::cerr << "[DuplicationCapture] Failed to duplicate output. Is another capture app running?" << std::endl;
            return false;
        }
        
        return true;
    }

    void release() {
        duplication.Reset();
    }

    cv::Mat getNextFrame(int timeoutMs, bool cpuReadback) {
        if (!duplication) return cv::Mat();

        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        ComPtr<IDXGIResource> desktopResource;
        
        HRESULT hr = duplication->AcquireNextFrame(timeoutMs, &frameInfo, desktopResource.GetAddressOf());
        
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return cv::Mat(); // No new frame
        }

        if (FAILED(hr)) {
            // Device lost or other error, signal need to reset
            if (hr == DXGI_ERROR_ACCESS_LOST) {
                std::cerr << "[DuplicationCapture] Access lost, need reset" << std::endl;
                release(); // Will force re-init next time
            }
            return cv::Mat();
        }

        // We got a frame, process it
        ComPtr<ID3D11Texture2D> texture;
        hr = desktopResource.As(&texture);
        
        cv::Mat result;
        if (SUCCEEDED(hr)) {
            // Keep reference for GPU path
            lastTexture = texture;
            if (cpuReadback) {
                result = processTexture(texture.Get());
            }
        }

        duplication->ReleaseFrame();
        return result;
    }

    cv::Mat processTexture(ID3D11Texture2D* texture) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        // Ensure staging texture matches
        if (!stagingTexture || 
            stagingDesc.Width != desc.Width || 
            stagingDesc.Height != desc.Height) {
            
            stagingDesc = desc;
            stagingDesc.BindFlags = 0;
            stagingDesc.MiscFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.Usage = D3D11_USAGE_STAGING;

            if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf()))) {
                return cv::Mat();
            }
        }

        context->CopyResource(stagingTexture.Get(), texture);

        D3D11_MAPPED_SUBRESOURCE map;
        if (FAILED(context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &map))) {
            return cv::Mat();
        }

        // Wrap data
        cv::Mat wrapped(desc.Height, desc.Width, CV_8UC4, map.pData, map.RowPitch);
        
        // Copy/Convert to BGR
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_BGRA2BGR);

        context->Unmap(stagingTexture.Get(), 0);

        return bgr;
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> lastTexture;
    
    ComPtr<ID3D11Texture2D> stagingTexture;
    D3D11_TEXTURE2D_DESC stagingDesc{};
};

DuplicationCapture::DuplicationCapture()
    : impl_(std::make_unique<Impl>()) {}

DuplicationCapture::~DuplicationCapture() {
    release();
}

bool DuplicationCapture::initialize() {
    if (initialized_) return true;

    if (impl_->initialize(monitorIndex_)) {
        initialized_ = true;
        return true;
    }
    return false;
}

void DuplicationCapture::release() {
    if (impl_) {
        impl_->release();
    }
    initialized_ = false;
}

Frame DuplicationCapture::getNextFrame() {
    if (!impl_) return Frame();
    
    // Auto-reinitialize if lost
    if (!initialized_) {
        // Try to re-init
        if (!initialize()) {
            return Frame();
        }
    }

    // Wait up to 100ms for a frame
    cv::Mat image = impl_->getNextFrame(100, cpuReadback_);
    
    // If cpuReadback is false, image will be empty, which is expected.
    // However, if we failed to get a frame (timeout), we still return empty.
    // We check if we got a texture to know if it was a success.
    if (!impl_->lastTexture) return Frame(); // No frame captured

    Frame frame(std::move(image), frameCounter_++);
    frame.gpuTexture = impl_->lastTexture;
    
    // Handle cropping (CPU only)
    if (cpuReadback_ && regionWidth_ > 0 && regionHeight_ > 0 && !frame.image.empty()) {
        int x = std::max(0, regionX_);
        int y = std::max(0, regionY_);
        
        if (x + regionWidth_ > frame.image.cols) x = std::max(0, frame.image.cols - regionWidth_);
        if (y + regionHeight_ > frame.image.rows) y = std::max(0, frame.image.rows - regionHeight_);
        
        frame.image = frame.image(cv::Rect(x, y, regionWidth_, regionHeight_)).clone();
        frame.width = regionWidth_;
        frame.height = regionHeight_;
        frame.roiX = x;
        frame.roiY = y;
    } else {
        // GPU Path: Just set the dimensions/offsets we want
        if (regionWidth_ > 0 && regionHeight_ > 0) {
            frame.width = regionWidth_;
            frame.height = regionHeight_;
            frame.roiX = regionX_;
            frame.roiY = regionY_;
        } else {
             // Full screen
             if (impl_->lastTexture) {
                 D3D11_TEXTURE2D_DESC desc;
                 impl_->lastTexture->GetDesc(&desc);
                 frame.width = desc.Width;
                 frame.height = desc.Height;
             } else {
                 frame.width = frame.image.cols;
                 frame.height = frame.image.rows;
             }
             frame.roiX = 0;
             frame.roiY = 0;
        }
    }

    return frame;
}

Frame DuplicationCapture::getGpuFrame() {
    if (!impl_ || !impl_->lastTexture) return Frame();
    
    Frame frame;
    frame.gpuTexture = impl_->lastTexture;
    frame.frameId = frameCounter_; // Approximation or sync with counter
    frame.timestamp = std::chrono::steady_clock::now();
    
    // Set dimensions from current capture
    if (regionWidth_ > 0) {
        frame.width = regionWidth_;
        frame.height = regionHeight_;
    } else {
        D3D11_TEXTURE2D_DESC desc;
        impl_->lastTexture->GetDesc(&desc);
        frame.width = desc.Width;
        frame.height = desc.Height;
    }
    
    return frame;
}

bool DuplicationCapture::isAvailable() const {
    return true; 
}

void DuplicationCapture::setRegion(int x, int y, int width, int height) {
    if (x == 0 && y == 0 && width == 0 && height == 0) return;
    
    regionX_ = x;
    regionY_ = y;
    regionWidth_ = width;
    regionHeight_ = height;
}

void DuplicationCapture::setTargetFps(int fps) {
    targetFps_ = fps;
}

void DuplicationCapture::setMonitor(int index) {
    monitorIndex_ = index;
    if (initialized_) {
        release();
        initialize();
    }
}

} // namespace sunone
