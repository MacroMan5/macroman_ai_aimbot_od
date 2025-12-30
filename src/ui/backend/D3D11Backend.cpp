#include "D3D11Backend.h"
#include <stdexcept>
#include <iostream>

namespace macroman {

D3D11Backend::D3D11Backend() = default;

D3D11Backend::~D3D11Backend() {
    shutdown();
}

bool D3D11Backend::initialize(HWND hwnd, int width, int height) {
    std::cout << "[D3D11Backend] Initializing..." << std::endl;

    if (hwnd == nullptr) {
        std::cerr << "[D3D11Backend] HWND is null" << std::endl;
        return false;
    }

    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    std::cout << "[D3D11Backend] HWND: " << hwnd << ", Size: " << width << "x" << height << std::endl;

    // Create device and context
    std::cout << "[D3D11Backend] Creating D3D11 device..." << std::endl;
    D3D_FEATURE_LEVEL featureLevel;
    UINT createFlags = 0;

#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
    std::cout << "[D3D11Backend] Debug mode enabled" << std::endl;
#endif

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel,
        &context_
    );

    if (FAILED(hr)) {
        std::cerr << "[D3D11Backend] D3D11CreateDevice failed: 0x" << std::hex << hr << std::endl;
        return false;
    }
    std::cout << "[D3D11Backend] D3D11 device created successfully" << std::endl;

    // Get DXGI factory
    ComPtr<IDXGIDevice> dxgiDevice;
    device_.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChainForHwnd(
        device_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain_
    );

    if (FAILED(hr)) {
        return false;
    }

    createRenderTarget();
    initialized_ = true;
    return true;
}

void D3D11Backend::shutdown() {
    releaseRenderTarget();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    initialized_ = false;
}

void D3D11Backend::beginFrame() {
    if (!initialized_) {
        std::cerr << "[D3D11Backend::beginFrame] NOT INITIALIZED - SKIPPING" << std::endl;
        return;
    }
    // Bind render target
    context_->OMSetRenderTargets(1, renderTarget_.GetAddressOf(), nullptr);

    // Set viewport
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(width_);
    vp.Height = static_cast<FLOAT>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context_->RSSetViewports(1, &vp);

    // Transparent black for layered window
    float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    context_->ClearRenderTargetView(renderTarget_.Get(), clearColor);
}

void D3D11Backend::endFrame() {
    if (!initialized_) return;
    HRESULT hr = swapChain_->Present(1, 0);  // VSync
    if (FAILED(hr)) {
        std::cerr << "[D3D11Backend::endFrame] Present FAILED: 0x" << std::hex << hr << std::endl;
    }
}

void D3D11Backend::resize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }

    if (!initialized_) {
        return;
    }

    width_ = width;
    height_ = height;

    releaseRenderTarget();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createRenderTarget();
}

void D3D11Backend::createRenderTarget() {
    if (!swapChain_) {
        return;
    }
    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTarget_);
    }
}

void D3D11Backend::releaseRenderTarget() {
    renderTarget_.Reset();
}

} // namespace macroman
