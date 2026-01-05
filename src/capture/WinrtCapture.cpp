#include "WinrtCapture.h"

// Include WinRT headers BEFORE Windows.h to avoid namespace conflicts
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11.h>
#include <dxgi.h>
// Include Windows.h AFTER WinRT to avoid namespace collision
#include <Windows.h>

#include <opencv2/imgproc.hpp>
#include <atomic>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Remove using namespace directives to avoid conflicts
// using namespace winrt;
// using namespace Windows::Foundation;
// ...

using namespace Windows::Graphics::DirectX::Direct3D11;

// Forward declaration of the interop interface if missing
extern "C" {
    struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetInterface(const GUID& id, void** object) = 0;
    };
}

namespace macroman {

class WinrtCapture::Impl {
public:
    Impl() {
        try {
            // Initialize D3D Device
            UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

            // Use standard device creation
            HRESULT hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put());

            if (FAILED(hr)) {
                std::cerr << "[WinrtCapture] Failed to create D3D11 device: 0x" << std::hex << hr << std::endl;
                throw std::runtime_error("D3D11 device creation failed");
            }

            // Get DXGI device for WinRT interop
            winrt::com_ptr<IDXGIDevice> dxgiDevice;
            hr = device->QueryInterface(IID_PPV_ARGS(dxgiDevice.put()));
            if (FAILED(hr)) {
                std::cerr << "[WinrtCapture] Failed to get DXGI device: 0x" << std::hex << hr << std::endl;
                throw std::runtime_error("DXGI device query failed");
            }

            // Create WinRT IInspectable wrapper for D3D device
            winrt::com_ptr<IInspectable> deviceInspectable;
            hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), deviceInspectable.put());
            if (FAILED(hr)) {
                std::cerr << "[WinrtCapture] Failed to create Direct3D11 device from DXGI: 0x" << std::hex << hr << std::endl;
                throw std::runtime_error("Direct3D11 device creation failed");
            }

            d3dDevice = deviceInspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
            if (!d3dDevice) {
                std::cerr << "[WinrtCapture] Failed to cast to IDirect3DDevice" << std::endl;
                throw std::runtime_error("IDirect3DDevice cast failed");
            }
        } catch (const winrt::hresult_error& e) {
            std::cerr << "[WinrtCapture] WinRT error in constructor: " << winrt::to_string(e.message()) << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "[WinrtCapture] Exception in constructor: " << e.what() << std::endl;
            throw;
        }
    }

    ~Impl() {
        stop();
    }

    bool start(const std::string& windowTitle) {
        std::lock_guard<std::mutex> lock(mutex);
        try {
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };

            if (!windowTitle.empty()) {
                // Find window handle (Unicode support)
                std::wstring wTitle(windowTitle.begin(), windowTitle.end());
                HWND hwnd = FindWindowW(nullptr, wTitle.c_str());

                if (!hwnd) {
                    std::cerr << "[WinrtCapture] Window not found: " << windowTitle << std::endl;
                    return false;
                }

                // Create item from window
                try {
                    auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
                    auto interop = activationFactory.as<IGraphicsCaptureItemInterop>();

                    HRESULT hr = interop->CreateForWindow(
                        hwnd,
                        winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
                        winrt::put_abi(item)
                    );

                    if (FAILED(hr)) {
                        std::cerr << "[WinrtCapture] CreateForWindow failed: 0x" << std::hex << hr << std::endl;
                        return false;
                    }
                } catch (const winrt::hresult_error& e) {
                    std::cerr << "[WinrtCapture] WinRT error creating window capture: "
                              << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                    return false;
                }
            } else {
                // Monitor capture (primary monitor for now)
                try {
                    auto activationFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
                    auto interop = activationFactory.as<IGraphicsCaptureItemInterop>();

                    // Get primary monitor handle
                    POINT pt = { 0, 0 };
                    HMONITOR hmon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);

                    if (!hmon) {
                        std::cerr << "[WinrtCapture] Failed to get primary monitor handle" << std::endl;
                        return false;
                    }

                    HRESULT hr = interop->CreateForMonitor(
                        hmon,
                        winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
                        winrt::put_abi(item)
                    );

                    if (FAILED(hr)) {
                        std::cerr << "[WinrtCapture] CreateForMonitor failed: 0x" << std::hex << hr << std::endl;
                        return false;
                    }
                } catch (const winrt::hresult_error& e) {
                    std::cerr << "[WinrtCapture] WinRT error creating monitor capture: "
                              << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                    return false;
                }
            }

            if (!item) {
                std::cerr << "[WinrtCapture] GraphicsCaptureItem is null after creation" << std::endl;
                return false;
            }

            itemSize = item.Size();

            if (itemSize.Width <= 0 || itemSize.Height <= 0) {
                std::cerr << "[WinrtCapture] Invalid item size: " << itemSize.Width << "x" << itemSize.Height << std::endl;
                return false;
            }

            // Create frame pool
            try {
                // Use CreateFreeThreaded as per best practices for performance
                framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                    d3dDevice,
                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                    2,
                    itemSize);

                if (!framePool) {
                    std::cerr << "[WinrtCapture] Failed to create frame pool" << std::endl;
                    return false;
                }
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[WinrtCapture] WinRT error creating frame pool: "
                          << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                return false;
            }

            // Create session
            try {
                session = framePool.CreateCaptureSession(item);
                if (!session) {
                    std::cerr << "[WinrtCapture] Failed to create capture session" << std::endl;
                    return false;
                }
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[WinrtCapture] WinRT error creating capture session: "
                          << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                return false;
            }

            // Set up event handler
            try {
                framePool.FrameArrived({ this, &Impl::onFrameArrived });
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[WinrtCapture] WinRT error setting up event handler: "
                          << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                return false;
            }

            // Start capture
            try {
                session.StartCapture();
            } catch (const winrt::hresult_error& e) {
                std::cerr << "[WinrtCapture] WinRT error starting capture: "
                          << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
                return false;
            }

            std::cout << "[WinrtCapture] Successfully started capture (" << itemSize.Width << "x" << itemSize.Height << ")" << std::endl;
            return true;

        } catch (const winrt::hresult_error& e) {
            std::cerr << "[WinrtCapture] Unexpected WinRT error in start(): "
                      << winrt::to_string(e.message()) << " (0x" << std::hex << e.code() << ")" << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[WinrtCapture] Unexpected exception in start(): " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[WinrtCapture] Unknown exception in start()" << std::endl;
            return false;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex);
        try {
            if (session) {
                try {
                    session.Close();
                } catch (const winrt::hresult_error& e) {
                    std::cerr << "[WinrtCapture] Error closing session: "
                              << winrt::to_string(e.message()) << std::endl;
                }
                session = nullptr;
            }
            if (framePool) {
                try {
                    framePool.Close();
                } catch (const winrt::hresult_error& e) {
                    std::cerr << "[WinrtCapture] Error closing frame pool: "
                              << winrt::to_string(e.message()) << std::endl;
                }
                framePool = nullptr;
            }
        } catch (...) {
            std::cerr << "[WinrtCapture] Unexpected error in stop()" << std::endl;
        }
    }

    cv::Mat getLatestFrame(bool cpuReadback) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!latestTexture) return cv::Mat();

        if (!cpuReadback) {
            return cv::Mat(); // Return empty mat if CPU access is not requested
        }

        // Lazy CPU copy
        try {
             D3D11_TEXTURE2D_DESC desc;
             latestTexture->GetDesc(&desc);

            // Ensure we have a staging texture for CPU access
            if (!stagingTexture ||
                desc.Width != stagingDesc.Width ||
                desc.Height != stagingDesc.Height) {

                stagingDesc = desc;
                stagingDesc.BindFlags = 0;
                stagingDesc.MiscFlags = 0;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc.Usage = D3D11_USAGE_STAGING;

                HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.put());
                if (FAILED(hr)) {
                    std::cerr << "[WinrtCapture] Failed to create staging texture: 0x" << std::hex << hr << std::endl;
                    return cv::Mat();
                }
            }

            // Copy to staging
            context->CopyResource(stagingTexture.get(), latestTexture.get());

            // Map and copy to OpenCV Mat
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT hr = context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(hr)) {
                std::cerr << "[WinrtCapture] Failed to map staging texture: 0x" << std::hex << hr << std::endl;
                return cv::Mat();
            }

            // Create Mat pointing to mapped data (or copy)
            cv::Mat wrapped(desc.Height, desc.Width, CV_8UC4, mapped.pData, mapped.RowPitch);

            // Convert to BGR for internal pipeline consistency
            cv::Mat bgr;
            cv::cvtColor(wrapped, bgr, cv::COLOR_BGRA2BGR);

            context->Unmap(stagingTexture.get(), 0);
            return bgr;

        } catch (const std::exception& e) {
            std::cerr << "[WinrtCapture] Error in getLatestFrame: " << e.what() << std::endl;
            return cv::Mat();
        }
    }

    winrt::com_ptr<ID3D11Texture2D> getLatestTexture() {
        std::lock_guard<std::mutex> lock(mutex);
        return latestTexture;
    }

    void onFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&) {
        try {
            auto frame = sender.TryGetNextFrame();
            if (!frame) return;

            std::lock_guard<std::mutex> lock(mutex);

            auto surface = frame.Surface();
            if (!surface) return;

            auto access = surface.as<IDirect3DDxgiInterfaceAccess>();
            if (!access) return;

            winrt::com_ptr<ID3D11Texture2D> texture;
            HRESULT hr = access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), winrt::put_abi(texture));
            if (FAILED(hr) || !texture) return;

            // Just store the texture reference! ZERO COPY!
            latestTexture = texture;

        } catch (...) {
            // Ignore errors in callback to avoid crash
        }
    }

    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice d3dDevice{ nullptr };
    
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{ nullptr };
    winrt::Windows::Graphics::SizeInt32 itemSize{ 0, 0 };

    winrt::com_ptr<ID3D11Texture2D> stagingTexture;
    D3D11_TEXTURE2D_DESC stagingDesc{};

    std::mutex mutex;
    winrt::com_ptr<ID3D11Texture2D> latestTexture;
};

WinrtCapture::WinrtCapture() {
    try {
        impl_ = std::make_unique<Impl>();
    } catch (const std::exception& e) {
        std::cerr << "[WinrtCapture] Failed to create WinRT capture implementation: " << e.what() << std::endl;
        impl_ = nullptr;
    }
}

WinrtCapture::~WinrtCapture() {
    release();
}

bool WinrtCapture::initialize() {
    if (initialized_) return true;

    if (!impl_) {
        std::cerr << "[WinrtCapture] Cannot initialize: implementation is null" << std::endl;
        return false;
    }

    try {
        // By default, capture primary monitor
        bool result = impl_->start(targetWindow_);
        if (result) {
            initialized_ = true;
            std::cout << "[WinrtCapture] Initialization successful" << std::endl;
        } else {
            std::cerr << "[WinrtCapture] Initialization failed" << std::endl;
        }
        return result;
    } catch (const std::exception& e) {
        std::cerr << "[WinrtCapture] Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void WinrtCapture::release() {
    try {
        if (impl_) {
            impl_->stop();
        }
        initialized_ = false;
    } catch (const std::exception& e) {
        std::cerr << "[WinrtCapture] Exception during release: " << e.what() << std::endl;
        initialized_ = false;
    }
}

Frame WinrtCapture::getNextFrame() {
    if (!initialized_ || !impl_) return Frame();

    try {
        // Get CPU frame only if requested
        cv::Mat image = impl_->getLatestFrame(cpuReadback_);
        auto texture = impl_->getLatestTexture();
        
        if (!texture) return Frame(); // No frame yet

        // If cpuReadback is false, image is empty.
        
        Frame frame(std::move(image), frameCounter_++);
        // Need to convert winrt::com_ptr to Microsoft::WRL::ComPtr or reinterpret_cast?
        // They are binary compatible (standard COM).
        
        // Safe casting via QueryInterface to be proper
        Microsoft::WRL::ComPtr<ID3D11Texture2D> wrlTexture;
        texture.as(IID_PPV_ARGS(wrlTexture.GetAddressOf()));
        frame.gpuTexture = wrlTexture;

        // Set dimensions (GPU path logic)
        if (regionWidth_ > 0 && regionHeight_ > 0) {
            frame.width = regionWidth_;
            frame.height = regionHeight_;
            frame.roiX = regionX_;
            frame.roiY = regionY_;
        } else {
            D3D11_TEXTURE2D_DESC desc;
            wrlTexture->GetDesc(&desc);
            frame.width = desc.Width;
            frame.height = desc.Height;
            frame.roiX = 0;
            frame.roiY = 0;
        }

        // Handle cropping if region is set (CPU ONLY)
        if (cpuReadback_ && regionWidth_ > 0 && regionHeight_ > 0 && !frame.image.empty()) {
            // Ensure bounds
            int x = std::max(0, regionX_);
            int y = std::max(0, regionY_);

            // Safe crop
            if (x + regionWidth_ > frame.image.cols) x = std::max(0, frame.image.cols - regionWidth_);
            if (y + regionHeight_ > frame.image.rows) y = std::max(0, frame.image.rows - regionHeight_);

            frame.image = frame.image(cv::Rect(x, y, regionWidth_, regionHeight_)).clone();
            // width/height/roiX/roiY already set above correctly
        }

        return frame;

    } catch (const cv::Exception& e) {
        std::cerr << "[WinrtCapture] OpenCV error in getNextFrame: " << e.what() << std::endl;
        return Frame();
    } catch (const std::exception& e) {
        std::cerr << "[WinrtCapture] Exception in getNextFrame: " << e.what() << std::endl;
        return Frame();
    }
}

bool WinrtCapture::isAvailable() const {
    // WinRT capture available on Windows 10 1803+ (17134)
    // We assume 1903+ for full feature set
    return true; 
}

void WinrtCapture::setRegion(int x, int y, int width, int height) {
    if (x == 0 && y == 0 && width == 0 && height == 0) return; // Ignore invalid
    
    regionX_ = x;
    regionY_ = y;
    regionWidth_ = width;
    regionHeight_ = height;
}

void WinrtCapture::setTargetFps(int fps) {
    targetFps_ = fps;
}

void WinrtCapture::setTargetWindow(const std::string& windowTitle) {
    targetWindow_ = windowTitle;
    if (initialized_) {
        try {
            release();
            if (!initialize()) {
                std::cerr << "[WinrtCapture] Failed to reinitialize with new target window" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[WinrtCapture] Exception setting target window: " << e.what() << std::endl;
        }
    }
}

void WinrtCapture::setCaptureEntireScreen(bool entire) {
    captureEntireScreen_ = entire;
}

} // namespace macroman