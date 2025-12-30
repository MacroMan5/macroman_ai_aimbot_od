#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>

namespace macroman {

using Microsoft::WRL::ComPtr;

class D3D11Backend {
public:
    D3D11Backend();
    ~D3D11Backend();

    bool initialize(HWND hwnd, int width, int height);
    void shutdown();

    void beginFrame();
    void endFrame();

    void resize(int width, int height);

    // Accessors (for ImGui/DirectComposition)
    ID3D11Device* getDevice() const { return device_.Get(); }
    ID3D11DeviceContext* getContext() const { return context_.Get(); }
    IDXGISwapChain1* getSwapChain() const { return swapChain_.Get(); }
    ID3D11RenderTargetView* getRenderTarget() const { return renderTarget_.Get(); }

    // State
    bool isInitialized() const { return initialized_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    void createRenderTarget();
    void releaseRenderTarget();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swapChain_;
    ComPtr<ID3D11RenderTargetView> renderTarget_;

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

} // namespace macroman
