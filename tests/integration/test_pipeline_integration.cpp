#include <catch2/catch_test_macros.hpp>
#include "../helpers/FakeScreenCapture.h"
#include "../helpers/FakeDetector.h"
#include <chrono>
#include <vector>

using namespace macroman;
using namespace macroman::test;

/**
 * @brief Pipeline Integration Tests
 *
 * Tests the full data flow from capture → detection using fake implementations.
 * Validates end-to-end latency, throughput, and correctness without requiring
 * actual GPU hardware or screen capture capabilities.
 */

TEST_CASE("Pipeline Integration - Capture to Detection Flow", "[integration][pipeline]") {
    FakeScreenCapture capture;
    FakeDetector detector;

    SECTION("Basic pipeline flow with synthetic frames and detections") {
        // Setup: 10 synthetic frames at 1920x1080
        capture.loadSyntheticFrames(10, 1920, 1080);
        REQUIRE(capture.initialize(nullptr));

        // Setup: Detector with predefined results
        detector.initialize("");

        Detection det;
        det.bbox = {960, 540, 100, 150};  // Center of screen
        det.confidence = 0.95f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        detector.loadPredefinedResults({det});

        // Process pipeline
        int framesProcessed = 0;
        int detectionsFound = 0;

        for (int i = 0; i < 10; ++i) {
            // Capture frame
            Frame frame = capture.captureFrame();
            REQUIRE(frame.width == 1920);
            REQUIRE(frame.height == 1080);

            // Detect
            DetectionList detections = detector.detect(frame);
            REQUIRE(detections.size() == 1);
            REQUIRE(detections[0].confidence == 0.95f);
            REQUIRE(detections[0].hitbox == HitboxType::Head);

            framesProcessed++;
            detectionsFound += static_cast<int>(detections.size());
        }

        REQUIRE(framesProcessed == 10);
        REQUIRE(detectionsFound == 10);
        REQUIRE(detector.getDetectCallCount() == 10);
    }

    SECTION("Pipeline with no detections (empty results)") {
        capture.loadSyntheticFrames(5, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");
        detector.loadPredefinedResults({});  // No detections

        int framesProcessed = 0;
        int detectionsFound = 0;

        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);

            REQUIRE(detections.empty());
            framesProcessed++;
            detectionsFound += static_cast<int>(detections.size());
        }

        REQUIRE(framesProcessed == 5);
        REQUIRE(detectionsFound == 0);
    }

    SECTION("Pipeline with multiple detections per frame") {
        capture.loadSyntheticFrames(5, 1920, 1080);
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");

        // Create 3 detections (head, chest, body)
        Detection det1;
        det1.bbox = {100, 100, 50, 80};
        det1.confidence = 0.9f;
        det1.classId = 0;
        det1.hitbox = HitboxType::Head;

        Detection det2;
        det2.bbox = {200, 150, 60, 90};
        det2.confidence = 0.85f;
        det2.classId = 1;
        det2.hitbox = HitboxType::Chest;

        Detection det3;
        det3.bbox = {300, 200, 70, 100};
        det3.confidence = 0.8f;
        det3.classId = 2;
        det3.hitbox = HitboxType::Body;

        detector.loadPredefinedResults({det1, det2, det3});

        int framesProcessed = 0;
        int totalDetections = 0;

        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);

            REQUIRE(detections.size() == 3);
            framesProcessed++;
            totalDetections += static_cast<int>(detections.size());
        }

        REQUIRE(framesProcessed == 5);
        REQUIRE(totalDetections == 15);  // 3 detections * 5 frames
    }
}

TEST_CASE("Pipeline Integration - Performance and Latency", "[integration][pipeline][performance]") {
    FakeScreenCapture capture;
    FakeDetector detector;

    SECTION("End-to-end latency measurement (no simulated delay)") {
        capture.loadSyntheticFrames(100, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");
        detector.setInferenceDelay(0.0f);  // Instant inference

        Detection det;
        det.bbox = {320, 320, 50, 80};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        detector.loadPredefinedResults({det});

        // Measure end-to-end latency
        std::vector<float> latencies;
        latencies.reserve(100);

        for (int i = 0; i < 100; ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            // Capture
            Frame frame = capture.captureFrame();

            // Detect
            DetectionList detections = detector.detect(frame);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            latencies.push_back(static_cast<float>(duration.count()) / 1000.0f);  // Convert to ms

            REQUIRE(detections.size() == 1);
        }

        // Calculate average latency
        float avgLatency = 0.0f;
        for (float lat : latencies) {
            avgLatency += lat;
        }
        avgLatency /= latencies.size();

        // With no delay, pipeline should be very fast (<1ms)
        REQUIRE(avgLatency < 1.0f);
    }

    SECTION("End-to-end latency with simulated inference delay (8ms)") {
        capture.loadSyntheticFrames(50, 640, 640);
        capture.setFrameRate(0);  // No capture delay
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");
        detector.setInferenceDelay(8.0f);  // Simulate 8ms inference (125 FPS detector)

        Detection det;
        det.bbox = {320, 320, 50, 80};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        detector.loadPredefinedResults({det});

        // Measure latencies
        std::vector<float> latencies;
        latencies.reserve(50);

        for (int i = 0; i < 50; ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            latencies.push_back(static_cast<float>(duration.count()));

            REQUIRE(detections.size() == 1);
        }

        // Calculate average
        float avgLatency = 0.0f;
        float maxLatency = 0.0f;
        for (float lat : latencies) {
            avgLatency += lat;
            if (lat > maxLatency) maxLatency = lat;
        }
        avgLatency /= latencies.size();

        // Average should be approximately 8ms (allow ±2ms tolerance)
        REQUIRE(avgLatency >= 6.0f);
        REQUIRE(avgLatency <= 10.0f);

        // Max latency should be within reason (±5ms tolerance)
        REQUIRE(maxLatency <= 15.0f);
    }

    SECTION("Throughput test - 500 frames golden dataset simulation") {
        capture.loadSyntheticFrames(500, 1920, 1080);
        capture.setFrameRate(0);  // Unlimited FPS
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");
        detector.setInferenceDelay(6.0f);  // Simulate ~166 FPS detector

        // Create realistic detection scenario (3 targets)
        Detection det1;
        det1.bbox = {320, 240, 40, 60};
        det1.confidence = 0.92f;
        det1.classId = 0;
        det1.hitbox = HitboxType::Head;

        Detection det2;
        det2.bbox = {500, 400, 50, 70};
        det2.confidence = 0.87f;
        det2.classId = 1;
        det2.hitbox = HitboxType::Chest;

        Detection det3;
        det3.bbox = {700, 300, 60, 80};
        det3.confidence = 0.83f;
        det3.classId = 2;
        det3.hitbox = HitboxType::Body;

        detector.loadPredefinedResults({det1, det2, det3});

        // Process all frames and measure total time
        auto startTime = std::chrono::high_resolution_clock::now();

        int framesProcessed = 0;
        int totalDetections = 0;

        for (int i = 0; i < 500; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);

            REQUIRE(detections.size() == 3);
            framesProcessed++;
            totalDetections += static_cast<int>(detections.size());
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        REQUIRE(framesProcessed == 500);
        REQUIRE(totalDetections == 1500);  // 3 detections * 500 frames

        // Calculate FPS
        float totalSeconds = static_cast<float>(totalDuration.count()) / 1000.0f;
        float avgFPS = static_cast<float>(framesProcessed) / totalSeconds;

        // With 6ms inference, should achieve ~150 FPS (allow margin for test overhead)
        REQUIRE(avgFPS >= 90.0f);  // At least 90 FPS (realistic with test framework overhead)
    }
}

TEST_CASE("Pipeline Integration - Confidence Filtering", "[integration][pipeline][filtering]") {
    FakeScreenCapture capture;
    FakeDetector detector;

    SECTION("Pipeline with confidence filtering enabled") {
        capture.loadSyntheticFrames(10, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        detector.initialize("");
        detector.setConfidenceFilteringEnabled(true);

        DetectorConfig config;
        config.confidenceThreshold = 0.7f;
        detector.setConfig(config);

        // Create detections with varying confidence
        Detection det1;  // High confidence (passes)
        det1.bbox = {100, 100, 50, 80};
        det1.confidence = 0.9f;
        det1.classId = 0;
        det1.hitbox = HitboxType::Head;

        Detection det2;  // Medium confidence (passes)
        det2.bbox = {200, 150, 60, 90};
        det2.confidence = 0.75f;
        det2.classId = 1;
        det2.hitbox = HitboxType::Chest;

        Detection det3;  // Low confidence (filtered out)
        det3.bbox = {300, 200, 70, 100};
        det3.confidence = 0.5f;
        det3.classId = 2;
        det3.hitbox = HitboxType::Body;

        detector.loadPredefinedResults({det1, det2, det3});

        int framesProcessed = 0;
        int totalDetections = 0;

        for (int i = 0; i < 10; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);

            // Only 2 detections should pass (det1 and det2)
            REQUIRE(detections.size() == 2);
            REQUIRE(detections[0].confidence >= 0.7f);
            REQUIRE(detections[1].confidence >= 0.7f);

            framesProcessed++;
            totalDetections += static_cast<int>(detections.size());
        }

        REQUIRE(framesProcessed == 10);
        REQUIRE(totalDetections == 20);  // 2 detections * 10 frames
    }
}

TEST_CASE("Pipeline Integration - Edge Cases and Robustness", "[integration][pipeline][edge]") {
    FakeScreenCapture capture;
    FakeDetector detector;

    SECTION("Pipeline handles rapid frame size changes") {
        // Simulate different resolutions
        std::vector<std::pair<uint32_t, uint32_t>> resolutions = {
            {640, 640},
            {1920, 1080},
            {2560, 1440},
            {640, 640}  // Back to original
        };

        detector.initialize("");

        Detection det;
        det.bbox = {100, 100, 50, 80};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        detector.loadPredefinedResults({det});

        for (const auto& res : resolutions) {
            capture.loadSyntheticFrames(5, res.first, res.second);
            REQUIRE(capture.initialize(nullptr));

            for (int i = 0; i < 5; ++i) {
                Frame frame = capture.captureFrame();
                REQUIRE(frame.width == res.first);
                REQUIRE(frame.height == res.second);

                DetectionList detections = detector.detect(frame);
                REQUIRE(detections.size() == 1);
            }

            capture.shutdown();
        }
    }

    SECTION("Pipeline handles detector reinitialization") {
        capture.loadSyntheticFrames(10, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        Detection det;
        det.bbox = {100, 100, 50, 80};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        // First initialization
        detector.initialize("model1.onnx");
        detector.loadPredefinedResults({det});

        // Process some frames
        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);
            REQUIRE(detections.size() == 1);
        }

        // Reinitialize detector
        detector.release();
        detector.initialize("model2.onnx");
        detector.loadPredefinedResults({det});

        // Process more frames
        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);
            REQUIRE(detections.size() == 1);
        }

        REQUIRE(detector.getDetectCallCount() == 10);
    }

    SECTION("Pipeline handles capture shutdown and restart") {
        detector.initialize("");

        Detection det;
        det.bbox = {100, 100, 50, 80};
        det.confidence = 0.9f;
        det.classId = 0;
        det.hitbox = HitboxType::Head;

        detector.loadPredefinedResults({det});

        // First capture session
        capture.loadSyntheticFrames(5, 640, 640);
        REQUIRE(capture.initialize(nullptr));

        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);
            REQUIRE(detections.size() == 1);
        }

        capture.shutdown();

        // Second capture session (different resolution)
        capture.loadSyntheticFrames(5, 1920, 1080);
        REQUIRE(capture.initialize(nullptr));

        for (int i = 0; i < 5; ++i) {
            Frame frame = capture.captureFrame();
            DetectionList detections = detector.detect(frame);
            REQUIRE(detections.size() == 1);
        }
    }
}

TEST_CASE("Pipeline Integration - Realistic Game Scenario", "[integration][pipeline][realistic]") {
    // Simulate a realistic gaming scenario:
    // - 144 FPS capture
    // - 60-144 FPS detection (varies with scene complexity)
    // - Multiple targets per frame
    // - Confidence filtering enabled
    // - 1000 frames processed

    FakeScreenCapture capture;
    FakeDetector detector;

    capture.loadSyntheticFrames(1000, 1920, 1080);
    capture.setFrameRate(144);  // 144 FPS capture
    REQUIRE(capture.initialize(nullptr));

    detector.initialize("game_model.onnx");
    detector.setInferenceDelay(7.0f);  // ~142 FPS detector
    detector.setConfidenceFilteringEnabled(true);

    DetectorConfig config;
    config.confidenceThreshold = 0.75f;
    detector.setConfig(config);

    // Create realistic multi-target scenario
    Detection det1;  // High confidence head (priority target)
    det1.bbox = {960, 400, 40, 60};
    det1.confidence = 0.95f;
    det1.classId = 0;
    det1.hitbox = HitboxType::Head;

    Detection det2;  // Medium confidence chest
    det2.bbox = {700, 500, 50, 70};
    det2.confidence = 0.82f;
    det2.classId = 1;
    det2.hitbox = HitboxType::Chest;

    Detection det3;  // Low confidence body (filtered out)
    det3.bbox = {1200, 600, 60, 80};
    det3.confidence = 0.65f;
    det3.classId = 2;
    det3.hitbox = HitboxType::Body;

    Detection det4;  // High confidence body
    det4.bbox = {500, 700, 70, 90};
    det4.confidence = 0.88f;
    det4.classId = 2;
    det4.hitbox = HitboxType::Body;

    detector.loadPredefinedResults({det1, det2, det3, det4});

    // Process frames
    auto startTime = std::chrono::high_resolution_clock::now();

    int framesProcessed = 0;
    int totalDetections = 0;
    std::vector<float> latencies;
    latencies.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        Frame frame = capture.captureFrame();
        DetectionList detections = detector.detect(frame);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        latencies.push_back(static_cast<float>(frameDuration.count()));

        // Should have 3 detections after filtering (det1, det2, det4)
        REQUIRE(detections.size() == 3);
        REQUIRE(detections[0].confidence >= 0.75f);
        REQUIRE(detections[1].confidence >= 0.75f);
        REQUIRE(detections[2].confidence >= 0.75f);

        framesProcessed++;
        totalDetections += static_cast<int>(detections.size());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Validate results
    REQUIRE(framesProcessed == 1000);
    REQUIRE(totalDetections == 3000);  // 3 detections * 1000 frames

    // Calculate performance metrics
    float totalSeconds = static_cast<float>(totalDuration.count()) / 1000.0f;
    float avgFPS = static_cast<float>(framesProcessed) / totalSeconds;

    float avgLatency = 0.0f;
    float maxLatency = 0.0f;
    for (float lat : latencies) {
        avgLatency += lat;
        if (lat > maxLatency) maxLatency = lat;
    }
    avgLatency /= latencies.size();

    // Performance assertions (adjusted for test environment with frame rate limiting)
    REQUIRE(avgFPS >= 60.0f);  // At least 60 FPS overall
    REQUIRE(avgLatency >= 5.0f);  // At least 5ms (capture + 7ms inference)
    REQUIRE(avgLatency <= 15.0f);  // Max 15ms average (includes 144 FPS frame limiting)
    REQUIRE(maxLatency <= 50.0f);  // Account for OS scheduler spikes in test environment
}
