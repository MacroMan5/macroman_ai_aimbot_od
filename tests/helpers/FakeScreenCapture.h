#pragma once

#include "core/interfaces/IScreenCapture.h"
#include "core/entities/Frame.h"
#include <vector>
#include <chrono>
#include <string>

namespace macroman {
namespace test {

/**
 * @brief Fake screen capture for integration testing
 *
 * Provides pre-recorded or synthetic frames for testing the pipeline
 * without requiring actual screen capture hardware.
 *
 * Usage:
 *   FakeScreenCapture capture;
 *   capture.loadSyntheticFrames(100, 640, 640);  // 100 frames, 640x640
 *   capture.initialize(nullptr);
 *
 *   Frame frame = capture.captureFrame();  // Returns synthetic frames in sequence
 *
 * Features:
 * - Synthetic frame generation (constant metadata, no actual pixels)
 * - Frame sequence looping (repeats when exhausted)
 * - Configurable frame rate simulation
 * - No GPU dependencies (suitable for headless CI/CD)
 *
 * Limitations:
 * - Returns frames with nullptr texture (frame.texture will be nullptr)
 * - Frame validity checked via width/height only (no actual pixel data)
 * - Not suitable for testing GPU-dependent code (use DuplicationCapture for that)
 */
class FakeScreenCapture : public IScreenCapture {
public:
    FakeScreenCapture() = default;
    ~FakeScreenCapture() override = default;

    // IScreenCapture interface
    bool initialize(void* targetWindowHandle) override;
    Frame captureFrame() override;
    void shutdown() override;
    [[nodiscard]] std::string getLastError() const override;

    /**
     * @brief Load synthetic frames (constant metadata, no pixels)
     * @param count Number of frames to generate
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     */
    void loadSyntheticFrames(size_t count, uint32_t width, uint32_t height);

    /**
     * @brief Set capture rate (for timing simulation)
     * @param fps Simulated frames per second (0 = no delay)
     */
    void setFrameRate(int fps) { targetFPS_ = fps; }

    /**
     * @brief Get total frames loaded
     */
    [[nodiscard]] size_t getFrameCount() const { return frames_.size(); }

    /**
     * @brief Get current frame index
     */
    [[nodiscard]] size_t getCurrentIndex() const { return currentIndex_; }

    /**
     * @brief Reset to first frame
     */
    void reset() { currentIndex_ = 0; frameSequence_ = 0; }

private:
    struct FakeFrameData {
        uint32_t width{0};
        uint32_t height{0};
        uint64_t frameId{0};
    };

    std::vector<FakeFrameData> frames_;
    size_t currentIndex_{0};
    uint64_t frameSequence_{0};
    int targetFPS_{0};  // 0 = no delay
    std::chrono::high_resolution_clock::time_point lastCaptureTime_;
    bool initialized_{false};
    std::string lastError_;
};

} // namespace test
} // namespace macroman
