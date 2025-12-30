#pragma once

#include <opencv2/core/mat.hpp>
#include <chrono>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11.h>

namespace sunone {

struct Frame {
    // Legacy CPU path
    cv::Mat image;
    
    // New GPU path - Using ComPtr for automatic reference counting
    // This ensures the texture stays alive even if the capture service moves to the next frame
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gpuTexture;

    std::chrono::steady_clock::time_point timestamp;
    uint64_t frameId;

    // Dimensions (valid for both paths)
    int width = 0;
    int height = 0;
    int roiX = 0;
    int roiY = 0;

    Frame() : frameId(0) {}

    Frame(cv::Mat img)
        : image(std::move(img))
        , timestamp(std::chrono::steady_clock::now())
        , frameId(0) {
            width = image.cols;
            height = image.rows;
        }

    Frame(cv::Mat img, uint64_t id)
        : image(std::move(img))
        , timestamp(std::chrono::steady_clock::now())
        , frameId(id) {
            width = image.cols;
            height = image.rows;
        }

    bool empty() const { return image.empty() && gpuTexture == nullptr; }
};

} // namespace sunone
