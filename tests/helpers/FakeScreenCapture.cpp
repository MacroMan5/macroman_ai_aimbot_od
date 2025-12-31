#include "FakeScreenCapture.h"
#include <thread>

namespace macroman {
namespace test {

bool FakeScreenCapture::initialize(void* /*targetWindowHandle*/) {
    if (frames_.empty()) {
        lastError_ = "No frames loaded. Call loadSyntheticFrames() first.";
        return false;
    }

    initialized_ = true;
    currentIndex_ = 0;
    frameSequence_ = 0;
    lastCaptureTime_ = std::chrono::high_resolution_clock::now();
    lastError_.clear();
    return true;
}

Frame FakeScreenCapture::captureFrame() {
    if (!initialized_) {
        lastError_ = "Not initialized. Call initialize() first.";
        return Frame{};  // Return invalid frame
    }

    if (frames_.empty()) {
        lastError_ = "No frames available.";
        return Frame{};
    }

    // Simulate frame rate (if configured)
    if (targetFPS_ > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration<double>(1.0 / targetFPS_);
        auto elapsed = std::chrono::duration<double>(now - lastCaptureTime_);

        if (elapsed < frameDuration) {
            // Sleep to match target FPS
            auto sleepTime = frameDuration - elapsed;
            std::this_thread::sleep_for(sleepTime);
        }

        lastCaptureTime_ = std::chrono::high_resolution_clock::now();
    }

    // Get current frame data
    const auto& frameData = frames_[currentIndex_];

    // Create frame with metadata only (no texture)
    Frame frame;
    frame.texture = nullptr;  // No actual GPU texture in test environment
    frame.width = frameData.width;
    frame.height = frameData.height;
    frame.frameSequence = frameSequence_++;
    frame.captureTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    // Advance to next frame (loop if at end)
    currentIndex_ = (currentIndex_ + 1) % frames_.size();

    return frame;
}

void FakeScreenCapture::shutdown() {
    initialized_ = false;
    currentIndex_ = 0;
    frameSequence_ = 0;
    lastError_.clear();
}

std::string FakeScreenCapture::getLastError() const {
    return lastError_;
}

void FakeScreenCapture::loadSyntheticFrames(size_t count, uint32_t width, uint32_t height) {
    frames_.clear();
    frames_.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        FakeFrameData data;
        data.width = width;
        data.height = height;
        data.frameId = i;
        frames_.push_back(data);
    }

    currentIndex_ = 0;
    frameSequence_ = 0;
}

} // namespace test
} // namespace macroman
