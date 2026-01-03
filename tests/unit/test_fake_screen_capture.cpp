#include <catch2/catch_test_macros.hpp>
#include "../helpers/FakeScreenCapture.h"
#include <chrono>

using namespace macroman;
using namespace macroman::test;

TEST_CASE("FakeScreenCapture - Basic functionality", "[integration][fake]") {
    FakeScreenCapture capture;

    SECTION("Uninitialized capture returns invalid frame") {
        Frame frame = capture.captureFrame();
        REQUIRE_FALSE(frame.isValid());
        REQUIRE(frame.empty());
    }

    SECTION("Initialize without frames fails") {
        bool result = capture.initialize(nullptr);
        REQUIRE_FALSE(result);
        REQUIRE_FALSE(capture.getLastError().empty());
    }

    SECTION("Load synthetic frames") {
        capture.loadSyntheticFrames(10, 640, 640);

        REQUIRE(capture.getFrameCount() == 10);
        REQUIRE(capture.getCurrentIndex() == 0);
    }

    SECTION("Capture synthetic frames in sequence") {
        capture.loadSyntheticFrames(5, 1920, 1080);
        REQUIRE(capture.initialize(nullptr));

        // Capture all 5 frames
        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();

            // Note: frame.isValid() will return false because texture is nullptr
            // But we can verify metadata is correct
            REQUIRE(frame.width == 1920);
            REQUIRE(frame.height == 1080);
            REQUIRE(frame.frameSequence == static_cast<uint64_t>(i));
            REQUIRE(frame.captureTimeNs > 0);
        }

        // 6th capture should loop back to frame 0
        Frame frame6 = capture.captureFrame();
        REQUIRE(frame6.width == 1920);
        REQUIRE(frame6.height == 1080);
        REQUIRE(frame6.frameSequence == 5);  // Sequence continues incrementing
    }

    SECTION("Frame sequence increments correctly") {
        capture.loadSyntheticFrames(3, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        Frame f1 = capture.captureFrame();
        Frame f2 = capture.captureFrame();
        Frame f3 = capture.captureFrame();
        Frame f4 = capture.captureFrame();  // Loops back

        REQUIRE(f1.frameSequence == 0);
        REQUIRE(f2.frameSequence == 1);
        REQUIRE(f3.frameSequence == 2);
        REQUIRE(f4.frameSequence == 3);  // Sequence continues
    }

    SECTION("Reset functionality") {
        capture.loadSyntheticFrames(10, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        // Capture a few frames
        capture.captureFrame();
        capture.captureFrame();
        capture.captureFrame();

        REQUIRE(capture.getCurrentIndex() == 3);

        // Reset
        capture.reset();

        REQUIRE(capture.getCurrentIndex() == 0);

        Frame frame = capture.captureFrame();
        REQUIRE(frame.frameSequence == 0);  // Sequence also reset
    }

    SECTION("Shutdown clears state") {
        capture.loadSyntheticFrames(10, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        capture.captureFrame();
        capture.captureFrame();

        capture.shutdown();

        // After shutdown, capture should return invalid frame
        Frame frame = capture.captureFrame();
        REQUIRE_FALSE(frame.isValid());
    }
}

TEST_CASE("FakeScreenCapture - Frame rate simulation", "[integration][fake][timing]") {
    FakeScreenCapture capture;

    SECTION("No frame rate limit (targetFPS = 0)") {
        capture.loadSyntheticFrames(100, 640, 640);
        capture.setFrameRate(0);  // No delay
        REQUIRE(capture.initialize(nullptr));

        auto start = std::chrono::high_resolution_clock::now();

        // Capture 100 frames as fast as possible
        for (int i = 0; i < 100; ++i) {
            capture.captureFrame();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should be very fast (< 10ms for 100 frames)
        REQUIRE(duration.count() < 100);
    }

    SECTION("Frame rate limiting (30 FPS)") {
        capture.loadSyntheticFrames(10, 640, 640);
        capture.setFrameRate(30);  // 30 FPS = ~33ms per frame
        REQUIRE(capture.initialize(nullptr));

        auto start = std::chrono::high_resolution_clock::now();

        // Capture 10 frames
        for (int i = 0; i < 10; ++i) {
            capture.captureFrame();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should take approximately 10 frames * 33ms = 330ms
        // Allow ±100ms tolerance
        REQUIRE(duration.count() >= 250);  // At least 250ms
        REQUIRE(duration.count() <= 450);  // At most 450ms
    }
}

TEST_CASE("FakeScreenCapture - Edge cases", "[integration][fake][edge]") {
    FakeScreenCapture capture;

    SECTION("Load zero frames") {
        capture.loadSyntheticFrames(0, 640, 640);

        REQUIRE(capture.getFrameCount() == 0);
        REQUIRE_FALSE(capture.initialize(nullptr));
    }

    SECTION("Load single frame (loops to itself)") {
        capture.loadSyntheticFrames(1, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        Frame f1 = capture.captureFrame();
        Frame f2 = capture.captureFrame();
        Frame f3 = capture.captureFrame();

        REQUIRE(f1.frameSequence == 0);
        REQUIRE(f2.frameSequence == 1);  // Sequence increments
        REQUIRE(f3.frameSequence == 2);  // Even though same frame data

        // All have same dimensions (looping single frame)
        REQUIRE(f1.width == 640);
        REQUIRE(f2.width == 640);
        REQUIRE(f3.width == 640);
    }

    SECTION("Multiple initialize/shutdown cycles") {
        capture.loadSyntheticFrames(5, 640, 640);

        // Cycle 1
        REQUIRE(capture.initialize(nullptr));
        capture.captureFrame();
        capture.captureFrame();
        capture.shutdown();

        // Cycle 2
        REQUIRE(capture.initialize(nullptr));
        Frame frame = capture.captureFrame();
        REQUIRE(frame.frameSequence == 0);  // Reset after shutdown
        capture.shutdown();

        // Cycle 3
        REQUIRE(capture.initialize(nullptr));
        frame = capture.captureFrame();
        REQUIRE(frame.frameSequence == 0);
    }

    SECTION("Different resolutions") {
        struct TestRes {
            uint32_t width, height;
        };

        TestRes resolutions[] = {
            {640, 640},
            {1920, 1080},
            {2560, 1440},
            {3840, 2160}
        };

        for (const auto& res : resolutions) {
            capture.loadSyntheticFrames(1, res.width, res.height);
            REQUIRE(capture.initialize(nullptr));

            Frame frame = capture.captureFrame();
            REQUIRE(frame.width == res.width);
            REQUIRE(frame.height == res.height);

            capture.shutdown();
        }
    }
}

TEST_CASE("FakeScreenCapture - Realistic usage scenario", "[integration][fake][realistic]") {
    // Scenario: Simulating 500 frames at 144 FPS for golden dataset testing
    FakeScreenCapture capture;

    capture.loadSyntheticFrames(500, 1920, 1080);
    capture.setFrameRate(144);  // 144 FPS
    REQUIRE(capture.initialize(nullptr));

    int framesProcessed = 0;
    uint64_t lastSequence = 0;

    // Process all 500 frames
    for (int i = 0; i < 500; ++i) {
        Frame frame = capture.captureFrame();

        // Verify metadata
        REQUIRE(frame.width == 1920);
        REQUIRE(frame.height == 1080);

        // First frame should be sequence 0, then increment
        if (i == 0) {
            REQUIRE(frame.frameSequence == 0);
        } else {
            REQUIRE(frame.frameSequence == lastSequence + 1);
        }
        REQUIRE(frame.captureTimeNs > 0);

        lastSequence = frame.frameSequence;
        ++framesProcessed;
    }

    REQUIRE(framesProcessed == 500);
    REQUIRE(lastSequence == 499);

    capture.shutdown();
}
