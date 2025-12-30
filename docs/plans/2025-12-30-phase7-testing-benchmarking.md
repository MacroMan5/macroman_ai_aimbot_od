# Phase 7: Testing & Benchmarking Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build comprehensive automated test suite with unit tests, integration tests, CLI benchmark tool, and CI/CD pipeline for performance regression testing.

**Architecture:** TDD approach with Catch2 for unit tests, fake implementations for integration testing, standalone CLI benchmark tool for headless performance validation, and GitHub Actions for automated CI/CD.

**Tech Stack:** Catch2 v3, Google Benchmark 1.8+, GitHub Actions, spdlog (logging), nlohmann/json (data serialization)

**Reference Documents:**
- @docs/plans/2025-12-29-modern-aimbot-architecture-design.md (Testing & Observability section)
- @docs/CRITICAL_TRAPS.md (Validation requirements)
- @CLAUDE.md (Testing requirements)

**Performance Targets:**
- All unit tests pass on clean build
- Benchmark achieves 120+ FPS average
- P99 latency < 15ms
- Zero allocations in hot path (validated via profiling)

---

## Task 1: Catch2 Framework Setup

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/unit/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root)
- Create: `tests/unit/test_main.cpp`

### Step 1: Write Catch2 test main file

**File:** `tests/unit/test_main.cpp`

```cpp
// Catch2 v3 main - define once in entire test suite
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

// This file only contains the main() for Catch2
// Individual test files will include catch_test_macros.hpp
```

### Step 2: Create tests CMakeLists.txt

**File:** `tests/CMakeLists.txt`

```cmake
# Tests root CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

# Find Catch2
find_package(Catch2 3 REQUIRED)

# Add subdirectories
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(benchmark)

# Enable CTest integration
include(CTest)
include(Catch)
```

### Step 3: Create unit tests CMakeLists.txt

**File:** `tests/unit/CMakeLists.txt`

```cmake
# Unit tests
add_executable(unit_tests
    test_main.cpp
    # Tests will be added incrementally
)

target_link_libraries(unit_tests
    PRIVATE
        Catch2::Catch2
        macroman_core
        macroman_detection
        macroman_input
        spdlog::spdlog
)

target_include_directories(unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/extracted_modules
)

# Discover tests for CTest
catch_discover_tests(unit_tests)
```

### Step 4: Update root CMakeLists.txt

**File:** `CMakeLists.txt` (add after project() declaration)

```cmake
# Testing options
option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_BENCHMARKS "Build benchmark suite" OFF)

# Add tests if enabled
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### Step 5: Build and verify Catch2 setup

Run:
```bash
cmake -B build -S . -DENABLE_TESTS=ON
cmake --build build --config Release
```

Expected: Build succeeds, `unit_tests.exe` created

### Step 6: Run test to verify Catch2 works

Run:
```bash
cd build
ctest -C Release --output-on-failure
```

Expected: "No tests found" (correct - we haven't written tests yet)

### Step 7: Commit

```bash
git add tests/CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/test_main.cpp CMakeLists.txt
git commit -m "test: add Catch2 framework setup and test infrastructure"
```

---

## Task 2: Kalman Filter Unit Tests

**Files:**
- Create: `tests/unit/test_kalman_predictor.cpp`
- Reference: `extracted_modules/input/prediction/KalmanPredictor.h`

### Step 1: Write failing test for basic prediction

**File:** `tests/unit/test_kalman_predictor.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "input/prediction/KalmanPredictor.h"

using Catch::Approx;

TEST_CASE("Kalman Filter predicts linear motion", "[prediction][kalman]") {
    KalmanPredictor kalman;

    // Frame 1: target at (100, 100)
    kalman.update({100.0f, 100.0f}, 0.016f);

    // Frame 2: target moved 10px right (velocity = 625 px/s)
    kalman.update({110.0f, 100.0f}, 0.016f);

    // Predict 3 frames ahead (48ms)
    Vec2 predicted = kalman.predict(0.048f);

    // Expected: x = 110 + 625*0.048 = 140px
    REQUIRE(predicted.x == Approx(140.0f).epsilon(0.1));
    REQUIRE(predicted.y == Approx(100.0f).epsilon(0.1));
}

TEST_CASE("Kalman Filter handles stationary target", "[prediction][kalman]") {
    KalmanPredictor kalman;

    // 3 frames of stationary target
    kalman.update({100.0f, 100.0f}, 0.016f);
    kalman.update({100.0f, 100.0f}, 0.016f);
    kalman.update({100.0f, 100.0f}, 0.016f);

    Vec2 predicted = kalman.predict(0.048f);

    // Velocity should be ~0, position unchanged
    REQUIRE(predicted.x == Approx(100.0f).epsilon(1.0));
    REQUIRE(predicted.y == Approx(100.0f).epsilon(1.0));
}

TEST_CASE("Kalman Filter handles acceleration", "[prediction][kalman]") {
    KalmanPredictor kalman;

    // Accelerating target: 0, 5, 15, 30 (increasing velocity)
    kalman.update({100.0f, 100.0f}, 0.016f);
    kalman.update({105.0f, 100.0f}, 0.016f);
    kalman.update({115.0f, 100.0f}, 0.016f);
    kalman.update({130.0f, 100.0f}, 0.016f);

    Vec2 predicted = kalman.predict(0.016f);

    // Should predict continued acceleration
    REQUIRE(predicted.x > 130.0f);
    REQUIRE(predicted.y == Approx(100.0f).epsilon(1.0));
}
```

### Step 2: Add test to CMakeLists.txt

**File:** `tests/unit/CMakeLists.txt` (modify)

```cmake
add_executable(unit_tests
    test_main.cpp
    test_kalman_predictor.cpp  # Add this line
)
```

### Step 3: Run test to verify it fails

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R kalman
```

Expected: FAIL with "KalmanPredictor not found" or compilation error

### Step 4: Verify KalmanPredictor exists (or create stub)

If KalmanPredictor doesn't exist yet, create minimal stub:

**File:** `extracted_modules/input/prediction/KalmanPredictor.h`

```cpp
#pragma once
#include "core/entities/Vec2.h"

class KalmanPredictor {
public:
    void update(Vec2 observation, float dt);
    Vec2 predict(float dt) const;

private:
    Vec2 position_{0.0f, 0.0f};
    Vec2 velocity_{0.0f, 0.0f};
};
```

**File:** `extracted_modules/input/prediction/KalmanPredictor.cpp`

```cpp
#include "KalmanPredictor.h"

void KalmanPredictor::update(Vec2 observation, float dt) {
    // Simplified: just store position, calculate velocity
    velocity_ = (observation - position_) / dt;
    position_ = observation;
}

Vec2 KalmanPredictor::predict(float dt) const {
    return position_ + velocity_ * dt;
}
```

### Step 5: Run test to verify it passes

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R kalman
```

Expected: PASS (3/3 tests)

### Step 6: Commit

```bash
git add tests/unit/test_kalman_predictor.cpp tests/unit/CMakeLists.txt
git add extracted_modules/input/prediction/KalmanPredictor.h extracted_modules/input/prediction/KalmanPredictor.cpp
git commit -m "test: add Kalman filter prediction unit tests"
```

---

## Task 3: NMS Post-Processing Unit Tests

**Files:**
- Create: `tests/unit/test_nms.cpp`
- Reference: `extracted_modules/detection/postprocess/PostProcessor.h`

### Step 1: Write failing test for NMS overlap removal

**File:** `tests/unit/test_nms.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "detection/postprocess/PostProcessor.h"
#include "core/entities/Detection.h"

using Catch::Approx;

TEST_CASE("NMS removes overlapping detections", "[detection][nms]") {
    std::vector<Detection> detections = {
        {BBox{10, 10, 50, 50}, 0.9f, 0, HitboxType::Head},
        {BBox{15, 15, 55, 55}, 0.8f, 0, HitboxType::Head},  // Overlaps with first (IoU > 0.5)
        {BBox{200, 200, 250, 250}, 0.85f, 0, HitboxType::Chest}  // No overlap
    };

    auto filtered = PostProcessor::applyNMS(detections, 0.5f);

    // Should keep 2 detections: highest confidence from overlap + non-overlapping
    REQUIRE(filtered.size() == 2);
    REQUIRE(filtered[0].confidence == Approx(0.9f));  // Kept highest from overlap
    REQUIRE(filtered[1].confidence == Approx(0.85f));  // Non-overlapping
}

TEST_CASE("NMS keeps non-overlapping detections", "[detection][nms]") {
    std::vector<Detection> detections = {
        {BBox{10, 10, 50, 50}, 0.9f, 0, HitboxType::Head},
        {BBox{200, 200, 250, 250}, 0.85f, 0, HitboxType::Chest},
        {BBox{400, 400, 450, 450}, 0.7f, 0, HitboxType::Body}
    };

    auto filtered = PostProcessor::applyNMS(detections, 0.5f);

    // All should be kept (no overlaps)
    REQUIRE(filtered.size() == 3);
}

TEST_CASE("NMS handles empty input", "[detection][nms]") {
    std::vector<Detection> detections;
    auto filtered = PostProcessor::applyNMS(detections, 0.5f);
    REQUIRE(filtered.empty());
}

TEST_CASE("NMS calculates IoU correctly", "[detection][nms]") {
    BBox box1{0, 0, 100, 100};  // Area = 10,000
    BBox box2{50, 50, 150, 150}; // Area = 10,000, intersection = 50x50 = 2,500

    float iou = PostProcessor::calculateIoU(box1, box2);

    // IoU = 2,500 / (10,000 + 10,000 - 2,500) = 2,500 / 17,500 ≈ 0.143
    REQUIRE(iou == Approx(0.143f).epsilon(0.01));
}
```

### Step 2: Add test to CMakeLists.txt

**File:** `tests/unit/CMakeLists.txt` (modify)

```cmake
add_executable(unit_tests
    test_main.cpp
    test_kalman_predictor.cpp
    test_nms.cpp  # Add this line
)
```

### Step 3: Run test to verify it fails

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R nms
```

Expected: FAIL with "PostProcessor not found"

### Step 4: Create PostProcessor stub (minimal implementation)

**File:** `extracted_modules/detection/postprocess/PostProcessor.h`

```cpp
#pragma once
#include <vector>
#include "core/entities/Detection.h"

class PostProcessor {
public:
    static std::vector<Detection> applyNMS(
        const std::vector<Detection>& detections,
        float iouThreshold
    );

    static float calculateIoU(const BBox& box1, const BBox& box2);

    static void filterByConfidence(
        std::vector<Detection>& detections,
        float minConfidence
    );
};
```

**File:** `extracted_modules/detection/postprocess/PostProcessor.cpp`

```cpp
#include "PostProcessor.h"
#include <algorithm>

float PostProcessor::calculateIoU(const BBox& box1, const BBox& box2) {
    float x1 = std::max(box1.x, box2.x);
    float y1 = std::max(box1.y, box2.y);
    float x2 = std::min(box1.x + box1.width, box2.x + box2.width);
    float y2 = std::min(box1.y + box1.height, box2.y + box2.height);

    float intersectionArea = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
    float box1Area = box1.width * box1.height;
    float box2Area = box2.width * box2.height;
    float unionArea = box1Area + box2Area - intersectionArea;

    return (unionArea > 0.0f) ? (intersectionArea / unionArea) : 0.0f;
}

std::vector<Detection> PostProcessor::applyNMS(
    const std::vector<Detection>& detections,
    float iouThreshold
) {
    if (detections.empty()) return {};

    // Sort by confidence (descending)
    std::vector<Detection> sorted = detections;
    std::sort(sorted.begin(), sorted.end(),
        [](const Detection& a, const Detection& b) {
            return a.confidence > b.confidence;
        });

    std::vector<Detection> result;
    std::vector<bool> suppressed(sorted.size(), false);

    for (size_t i = 0; i < sorted.size(); ++i) {
        if (suppressed[i]) continue;

        result.push_back(sorted[i]);

        // Suppress overlapping boxes
        for (size_t j = i + 1; j < sorted.size(); ++j) {
            if (suppressed[j]) continue;

            float iou = calculateIoU(sorted[i].bbox, sorted[j].bbox);
            if (iou > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }

    return result;
}

void PostProcessor::filterByConfidence(
    std::vector<Detection>& detections,
    float minConfidence
) {
    detections.erase(
        std::remove_if(detections.begin(), detections.end(),
            [minConfidence](const Detection& d) {
                return d.confidence < minConfidence;
            }),
        detections.end()
    );
}
```

### Step 5: Run test to verify it passes

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R nms
```

Expected: PASS (4/4 tests)

### Step 6: Commit

```bash
git add tests/unit/test_nms.cpp tests/unit/CMakeLists.txt
git add extracted_modules/detection/postprocess/PostProcessor.h extracted_modules/detection/postprocess/PostProcessor.cpp
git commit -m "test: add NMS post-processing unit tests"
```

---

## Task 4: Bezier Curve Unit Tests

**Files:**
- Create: `tests/unit/test_bezier.cpp`
- Reference: `extracted_modules/input/movement/BezierCurve.h`

### Step 1: Write failing test for Bezier evaluation

**File:** `tests/unit/test_bezier.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "input/movement/BezierCurve.h"

using Catch::Approx;

TEST_CASE("Bezier curve evaluates endpoints correctly", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 5.0f};
    curve.p2 = {20.0f, 5.0f};
    curve.p3 = {30.0f, 0.0f};

    // At t=0, should return p0
    Vec2 start = curve.evaluate(0.0f);
    REQUIRE(start.x == Approx(0.0f));
    REQUIRE(start.y == Approx(0.0f));

    // At t=1, should return p3
    Vec2 end = curve.evaluate(1.0f);
    REQUIRE(end.x == Approx(30.0f));
    REQUIRE(end.y == Approx(0.0f));
}

TEST_CASE("Bezier curve interpolates smoothly", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 10.0f};
    curve.p2 = {20.0f, 10.0f};
    curve.p3 = {30.0f, 0.0f};

    // At t=0.5, should be near midpoint
    Vec2 mid = curve.evaluate(0.5f);
    REQUIRE(mid.x == Approx(15.0f).epsilon(0.1));
    REQUIRE(mid.y > 0.0f);  // Should be above line (convex curve)
}

TEST_CASE("Bezier curve is monotonic in x for aiming", "[input][bezier]") {
    BezierCurve curve;
    curve.p0 = {0.0f, 0.0f};
    curve.p1 = {10.0f, 5.0f};
    curve.p2 = {20.0f, 5.0f};
    curve.p3 = {30.0f, 0.0f};

    // Check that x increases monotonically (no backwards movement)
    Vec2 prev = curve.evaluate(0.0f);
    for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
        Vec2 current = curve.evaluate(t);
        REQUIRE(current.x >= prev.x);  // x should never decrease
        prev = current;
    }
}
```

### Step 2: Add test to CMakeLists.txt

**File:** `tests/unit/CMakeLists.txt` (modify)

```cmake
add_executable(unit_tests
    test_main.cpp
    test_kalman_predictor.cpp
    test_nms.cpp
    test_bezier.cpp  # Add this line
)
```

### Step 3: Run test to verify it fails

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R bezier
```

Expected: FAIL with "BezierCurve not found"

### Step 4: Create BezierCurve implementation

**File:** `extracted_modules/input/movement/BezierCurve.h`

```cpp
#pragma once
#include "core/entities/Vec2.h"

struct BezierCurve {
    Vec2 p0;  // Start point
    Vec2 p1;  // Control point 1
    Vec2 p2;  // Control point 2
    Vec2 p3;  // End point

    // Evaluate cubic Bezier at parameter t ∈ [0, 1]
    Vec2 evaluate(float t) const;

    // Create curve from start to end with overshoot
    static BezierCurve createWithOvershoot(
        Vec2 start,
        Vec2 end,
        float overshootFactor = 0.15f
    );
};
```

**File:** `extracted_modules/input/movement/BezierCurve.cpp`

```cpp
#include "BezierCurve.h"
#include <algorithm>

Vec2 BezierCurve::evaluate(float t) const {
    // Clamp t to [0, 1]
    t = std::clamp(t, 0.0f, 1.0f);

    // Cubic Bezier formula: B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
    float u = 1.0f - t;
    float uu = u * u;
    float uuu = uu * u;
    float tt = t * t;
    float ttt = tt * t;

    Vec2 result;
    result.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
    result.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;

    return result;
}

BezierCurve BezierCurve::createWithOvershoot(
    Vec2 start,
    Vec2 end,
    float overshootFactor
) {
    Vec2 direction = (end - start).normalized();
    Vec2 overshootTarget = end + direction * overshootFactor * (end - start).length();

    BezierCurve curve;
    curve.p0 = start;
    curve.p1 = start + (overshootTarget - start) * 0.3f;
    curve.p2 = start + (overshootTarget - start) * 0.7f;
    curve.p3 = overshootTarget;

    return curve;
}
```

### Step 5: Run test to verify it passes

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R bezier
```

Expected: PASS (3/3 tests)

### Step 6: Commit

```bash
git add tests/unit/test_bezier.cpp tests/unit/CMakeLists.txt
git add extracted_modules/input/movement/BezierCurve.h extracted_modules/input/movement/BezierCurve.cpp
git commit -m "test: add Bezier curve unit tests"
```

---

## Task 5: LatestFrameQueue Unit Tests (Critical Trap Validation)

**Files:**
- Create: `tests/unit/test_latest_frame_queue.cpp`
- Reference: `extracted_modules/core/threading/LatestFrameQueue.h`
- Reference: @docs/CRITICAL_TRAPS.md (Trap 1)

### Step 1: Write test for head-drop policy

**File:** `tests/unit/test_latest_frame_queue.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/threading/LatestFrameQueue.h"
#include <memory>

// Test item with automatic cleanup tracking
struct TestItem {
    int value;
    static int cleanupCount;

    TestItem(int v) : value(v) {}
    ~TestItem() { cleanupCount++; }
};

int TestItem::cleanupCount = 0;

TEST_CASE("LatestFrameQueue drops old frames", "[threading][queue]") {
    LatestFrameQueue<TestItem> queue;
    TestItem::cleanupCount = 0;

    // Push 3 items (should only keep latest)
    queue.push(std::make_unique<TestItem>(1));
    queue.push(std::make_unique<TestItem>(2));
    queue.push(std::make_unique<TestItem>(3));

    // Pop should return item 3 (latest)
    auto item = queue.pop();
    REQUIRE(item != nullptr);
    REQUIRE(item->value == 3);

    // Verify old items were cleaned up (2 destructor calls)
    REQUIRE(TestItem::cleanupCount == 2);
}

TEST_CASE("LatestFrameQueue returns nullptr when empty", "[threading][queue]") {
    LatestFrameQueue<TestItem> queue;
    auto item = queue.pop();
    REQUIRE(item == nullptr);
}

TEST_CASE("LatestFrameQueue handles rapid push/pop", "[threading][queue]") {
    LatestFrameQueue<TestItem> queue;
    TestItem::cleanupCount = 0;

    // Rapid pushes
    for (int i = 0; i < 100; ++i) {
        queue.push(std::make_unique<TestItem>(i));
    }

    // Should only get latest (99)
    auto item = queue.pop();
    REQUIRE(item != nullptr);
    REQUIRE(item->value == 99);

    // Verify all intermediate items cleaned up
    REQUIRE(TestItem::cleanupCount == 99);
}
```

### Step 2: Add test to CMakeLists.txt

**File:** `tests/unit/CMakeLists.txt` (modify)

```cmake
add_executable(unit_tests
    test_main.cpp
    test_kalman_predictor.cpp
    test_nms.cpp
    test_bezier.cpp
    test_latest_frame_queue.cpp  # Add this line
)
```

### Step 3: Run test to verify it fails

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R queue
```

Expected: FAIL (LatestFrameQueue not found)

### Step 4: Verify LatestFrameQueue implementation exists

If not, create minimal implementation:

**File:** `extracted_modules/core/threading/LatestFrameQueue.h`

```cpp
#pragma once
#include <atomic>
#include <memory>

template<typename T>
class LatestFrameQueue {
public:
    LatestFrameQueue() : slot_(nullptr) {}

    ~LatestFrameQueue() {
        auto* item = slot_.exchange(nullptr, std::memory_order_acquire);
        if (item) delete item;
    }

    // Push new item, dropping old one
    void push(std::unique_ptr<T> item) {
        T* old = slot_.exchange(item.release(), std::memory_order_release);
        if (old) {
            delete old;  // Cleanup old item
        }
    }

    // Pop latest item (returns nullptr if empty)
    std::unique_ptr<T> pop() {
        T* item = slot_.exchange(nullptr, std::memory_order_acquire);
        return std::unique_ptr<T>(item);
    }

private:
    std::atomic<T*> slot_;
};
```

### Step 5: Run test to verify it passes

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R queue
```

Expected: PASS (3/3 tests)

### Step 6: Commit

```bash
git add tests/unit/test_latest_frame_queue.cpp tests/unit/CMakeLists.txt
git add extracted_modules/core/threading/LatestFrameQueue.h
git commit -m "test: add LatestFrameQueue unit tests (validates Trap 1 mitigation)"
```

---

## Task 6: Integration Test Infrastructure

**Files:**
- Create: `tests/integration/CMakeLists.txt`
- Create: `tests/integration/fakes/FakeScreenCapture.h`
- Create: `tests/integration/fakes/FakeDetector.h`
- Create: `tests/integration/test_pipeline.cpp`

### Step 1: Create FakeScreenCapture

**File:** `tests/integration/fakes/FakeScreenCapture.h`

```cpp
#pragma once
#include "core/interfaces/IScreenCapture.h"
#include <vector>
#include <fstream>

class FakeScreenCapture : public IScreenCapture {
public:
    void loadRecording(const std::string& path) {
        // Load pre-recorded frames from disk
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to load recording: " + path);
        }

        // Read frame count
        size_t frameCount;
        file.read(reinterpret_cast<char*>(&frameCount), sizeof(frameCount));

        frames_.clear();
        frames_.reserve(frameCount);

        // Read frames (simplified - would include texture data)
        for (size_t i = 0; i < frameCount; ++i) {
            Frame frame;
            frame.sequence = i;
            frame.timestamp = std::chrono::high_resolution_clock::now();
            frames_.push_back(frame);
        }

        index_ = 0;
    }

    Frame* captureFrame() override {
        if (frames_.empty()) return nullptr;

        Frame* frame = &frames_[index_];
        index_ = (index_ + 1) % frames_.size();  // Loop
        return frame;
    }

    void initialize(HWND) override {}
    void shutdown() override {}

private:
    std::vector<Frame> frames_;
    size_t index_{0};
};
```

### Step 2: Create FakeDetector

**File:** `tests/integration/fakes/FakeDetector.h`

```cpp
#pragma once
#include "core/interfaces/IDetector.h"

class FakeDetector : public IDetector {
public:
    void setResults(std::vector<Detection> results) {
        results_ = std::move(results);
    }

    std::vector<Detection> detect(Frame* frame) override {
        detectCount_++;
        return results_;  // Return pre-configured results
    }

    void loadModel(const std::string&) override {}
    void shutdown() override {}

    size_t getDetectCount() const { return detectCount_; }

private:
    std::vector<Detection> results_;
    size_t detectCount_{0};
};
```

### Step 3: Write integration test

**File:** `tests/integration/test_pipeline.cpp`

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "fakes/FakeScreenCapture.h"
#include "fakes/FakeDetector.h"
#include "Pipeline.h"

using Catch::Approx;

TEST_CASE("Pipeline processes frames end-to-end", "[integration][pipeline]") {
    // Setup fake components
    auto capture = std::make_unique<FakeScreenCapture>();
    auto detector = std::make_unique<FakeDetector>();

    // Configure fake detector to return one detection
    detector->setResults({
        Detection{BBox{320, 240, 50, 80}, 0.9f, 0, HitboxType::Head}
    });

    // Create pipeline
    Pipeline pipeline(std::move(capture), std::move(detector));

    // Run for 100 frames
    pipeline.run(100);

    // Verify processing occurred
    REQUIRE(pipeline.getProcessedFrameCount() == 100);
    REQUIRE(pipeline.getAverageLatency() > 0.0f);
}

TEST_CASE("Pipeline meets latency target", "[integration][pipeline][performance]") {
    auto capture = std::make_unique<FakeScreenCapture>();
    auto detector = std::make_unique<FakeDetector>();

    detector->setResults({
        Detection{BBox{320, 240, 50, 80}, 0.9f, 0, HitboxType::Head}
    });

    Pipeline pipeline(std::move(capture), std::move(detector));
    pipeline.run(500);

    // Verify P95 latency < 15ms
    float p95Latency = pipeline.getP95Latency();
    REQUIRE(p95Latency < 15.0f);
}
```

### Step 4: Create integration CMakeLists.txt

**File:** `tests/integration/CMakeLists.txt`

```cmake
add_executable(integration_tests
    test_pipeline.cpp
)

target_link_libraries(integration_tests
    PRIVATE
        Catch2::Catch2
        macroman_core
        macroman_capture
        macroman_detection
        spdlog::spdlog
)

target_include_directories(integration_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/extracted_modules
        ${CMAKE_CURRENT_SOURCE_DIR}/fakes
)

catch_discover_tests(integration_tests)
```

### Step 5: Update root tests CMakeLists.txt

**File:** `tests/CMakeLists.txt` (already done in Task 1, verify)

### Step 6: Build and run integration tests

Run:
```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure -R integration
```

Expected: PASS (if Pipeline exists) or FAIL (need to implement Pipeline)

### Step 7: Commit

```bash
git add tests/integration/
git commit -m "test: add integration test infrastructure with fake implementations"
```

---

## Task 7: CLI Benchmark Tool - Basic Structure

**Files:**
- Create: `tests/benchmark/CMakeLists.txt`
- Create: `tests/benchmark/sunone-bench.cpp`
- Create: `tests/benchmark/BenchmarkRunner.h`
- Create: `tests/benchmark/BenchmarkRunner.cpp`

### Step 1: Write benchmark main

**File:** `tests/benchmark/sunone-bench.cpp`

```cpp
#include "BenchmarkRunner.h"
#include <iostream>
#include <string>
#include <cxxopts.hpp>  // Command-line argument parsing

int main(int argc, char** argv) {
    try {
        cxxopts::Options options("sunone-bench", "Macroman AI Aimbot Benchmark Tool");

        options.add_options()
            ("d,dataset", "Path to test dataset", cxxopts::value<std::string>())
            ("m,model", "Path to ONNX model", cxxopts::value<std::string>())
            ("threshold-avg-fps", "Minimum average FPS", cxxopts::value<float>()->default_value("120.0"))
            ("threshold-p99-latency", "Maximum P99 latency (ms)", cxxopts::value<float>()->default_value("15.0"))
            ("h,help", "Print usage");

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        if (!result.count("dataset") || !result.count("model")) {
            std::cerr << "Error: --dataset and --model are required\n";
            std::cout << options.help() << std::endl;
            return 1;
        }

        BenchmarkConfig config;
        config.datasetPath = result["dataset"].as<std::string>();
        config.modelPath = result["model"].as<std::string>();
        config.thresholdAvgFPS = result["threshold-avg-fps"].as<float>();
        config.thresholdP99Latency = result["threshold-p99-latency"].as<float>();

        BenchmarkRunner runner(config);
        BenchmarkResults results = runner.run();

        // Print results
        std::cout << "========================================\n";
        std::cout << "Benchmark Results\n";
        std::cout << "========================================\n";
        std::cout << "Frames Processed:    " << results.framesProcessed << "\n";
        std::cout << "Average FPS:         " << results.avgFPS;
        if (results.avgFPS >= config.thresholdAvgFPS) {
            std::cout << "  [✓ PASS: >= " << config.thresholdAvgFPS << "]\n";
        } else {
            std::cout << "  [✗ FAIL: < " << config.thresholdAvgFPS << "]\n";
        }

        std::cout << "P95 Latency:         " << results.p95Latency << "ms\n";
        std::cout << "P99 Latency:         " << results.p99Latency << "ms";
        if (results.p99Latency <= config.thresholdP99Latency) {
            std::cout << "  [✓ PASS: <= " << config.thresholdP99Latency << "]\n";
        } else {
            std::cout << "  [✗ FAIL: > " << config.thresholdP99Latency << "]\n";
        }

        std::cout << "Detected Targets:    " << results.totalDetections << "\n";
        std::cout << "========================================\n";

        bool passed = results.avgFPS >= config.thresholdAvgFPS &&
                      results.p99Latency <= config.thresholdP99Latency;

        std::cout << "VERDICT: " << (passed ? "PASS" : "FAIL") << "\n";

        return passed ? 0 : 1;  // Exit code for CI/CD

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
```

### Step 2: Write BenchmarkRunner header

**File:** `tests/benchmark/BenchmarkRunner.h`

```cpp
#pragma once
#include <string>
#include <vector>

struct BenchmarkConfig {
    std::string datasetPath;
    std::string modelPath;
    float thresholdAvgFPS;
    float thresholdP99Latency;
};

struct BenchmarkResults {
    size_t framesProcessed;
    float avgFPS;
    float p95Latency;
    float p99Latency;
    size_t totalDetections;
    std::vector<float> latencies;  // For percentile calculation
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchmarkConfig& config);
    BenchmarkResults run();

private:
    BenchmarkConfig config_;

    float calculatePercentile(const std::vector<float>& data, float percentile);
};
```

### Step 3: Write BenchmarkRunner implementation

**File:** `tests/benchmark/BenchmarkRunner.cpp`

```cpp
#include "BenchmarkRunner.h"
#include "Pipeline.h"
#include "integration/fakes/FakeScreenCapture.h"
#include "detection/directml/DMLDetector.h"
#include <algorithm>
#include <chrono>

BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& config)
    : config_(config) {}

BenchmarkResults BenchmarkRunner::run() {
    // Load dataset
    auto capture = std::make_unique<FakeScreenCapture>();
    capture->loadRecording(config_.datasetPath);

    // Load model
    auto detector = std::make_unique<DMLDetector>();
    detector->loadModel(config_.modelPath);

    // Create pipeline
    Pipeline pipeline(std::move(capture), std::move(detector));

    // Run benchmark
    auto startTime = std::chrono::high_resolution_clock::now();
    pipeline.run(500);  // Fixed 500 frames for consistency
    auto endTime = std::chrono::high_resolution_clock::now();

    // Calculate results
    BenchmarkResults results;
    results.framesProcessed = 500;

    float totalTime = std::chrono::duration<float>(endTime - startTime).count();
    results.avgFPS = results.framesProcessed / totalTime;

    // Get latencies from pipeline
    results.latencies = pipeline.getLatencies();
    results.p95Latency = calculatePercentile(results.latencies, 0.95f);
    results.p99Latency = calculatePercentile(results.latencies, 0.99f);

    results.totalDetections = pipeline.getTotalDetections();

    return results;
}

float BenchmarkRunner::calculatePercentile(const std::vector<float>& data, float percentile) {
    if (data.empty()) return 0.0f;

    std::vector<float> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(percentile * sorted.size());
    index = std::min(index, sorted.size() - 1);

    return sorted[index];
}
```

### Step 4: Create benchmark CMakeLists.txt

**File:** `tests/benchmark/CMakeLists.txt`

```cmake
add_executable(sunone-bench
    sunone-bench.cpp
    BenchmarkRunner.cpp
)

target_link_libraries(sunone-bench
    PRIVATE
        macroman_core
        macroman_capture
        macroman_detection
        spdlog::spdlog
        cxxopts::cxxopts
)

target_include_directories(sunone-bench
    PRIVATE
        ${CMAKE_SOURCE_DIR}/extracted_modules
        ${CMAKE_SOURCE_DIR}/tests/integration
)

# Install benchmark tool
install(TARGETS sunone-bench
    RUNTIME DESTINATION bin
)
```

### Step 5: Build benchmark tool

Run:
```bash
cmake --build build --config Release
```

Expected: `sunone-bench.exe` created in `build/bin/`

### Step 6: Test benchmark tool (without dataset yet)

Run:
```bash
./build/bin/sunone-bench.exe --help
```

Expected: Help message displayed

### Step 7: Commit

```bash
git add tests/benchmark/
git commit -m "feat: add CLI benchmark tool (sunone-bench) for performance regression testing"
```

---

## Task 8: Test Dataset Recording Utility

**Files:**
- Create: `tools/record_dataset.cpp`
- Create: `tools/CMakeLists.txt`
- Create: `test_data/.gitkeep`

### Step 1: Write dataset recorder

**File:** `tools/record_dataset.cpp`

```cpp
#include "capture/WinrtCapture.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

struct RecordConfig {
    std::string outputPath;
    size_t frameCount;
    HWND targetWindow;
};

class DatasetRecorder {
public:
    explicit DatasetRecorder(const RecordConfig& config)
        : config_(config) {}

    void record() {
        WinrtCapture capture;
        capture.initialize(config_.targetWindow);

        std::ofstream file(config_.outputPath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open output file: " + config_.outputPath);
        }

        // Write header
        file.write(reinterpret_cast<const char*>(&config_.frameCount), sizeof(config_.frameCount));

        std::cout << "Recording " << config_.frameCount << " frames...\n";

        for (size_t i = 0; i < config_.frameCount; ++i) {
            Frame* frame = capture.captureFrame();
            if (!frame) {
                std::cerr << "Warning: Failed to capture frame " << i << "\n";
                continue;
            }

            // Write frame data (simplified - would include texture data)
            uint64_t timestamp = frame->timestamp.time_since_epoch().count();
            file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
            file.write(reinterpret_cast<const char*>(&frame->sequence), sizeof(frame->sequence));

            if ((i + 1) % 50 == 0) {
                std::cout << "Progress: " << (i + 1) << "/" << config_.frameCount << "\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(7));  // ~144 FPS
        }

        capture.shutdown();
        std::cout << "Dataset saved to: " << config_.outputPath << "\n";
    }

private:
    RecordConfig config_;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: record_dataset <output_path> <frame_count>\n";
        std::cerr << "Example: record_dataset test_data/valorant_500frames.bin 500\n";
        return 1;
    }

    try {
        RecordConfig config;
        config.outputPath = argv[1];
        config.frameCount = std::stoul(argv[2]);
        config.targetWindow = GetForegroundWindow();  // Capture active window

        DatasetRecorder recorder(config);
        recorder.record();

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
```

### Step 2: Create tools CMakeLists.txt

**File:** `tools/CMakeLists.txt`

```cmake
add_executable(record_dataset
    record_dataset.cpp
)

target_link_libraries(record_dataset
    PRIVATE
        macroman_capture
        spdlog::spdlog
)

target_include_directories(record_dataset
    PRIVATE
        ${CMAKE_SOURCE_DIR}/extracted_modules
)

install(TARGETS record_dataset
    RUNTIME DESTINATION bin
)
```

### Step 3: Update root CMakeLists.txt

**File:** `CMakeLists.txt` (add after tests)

```cmake
# Add tools
add_subdirectory(tools)
```

### Step 4: Create test_data directory

Run:
```bash
mkdir -p test_data
touch test_data/.gitkeep
```

### Step 5: Build recorder tool

Run:
```bash
cmake --build build --config Release
```

Expected: `record_dataset.exe` created

### Step 6: Add .gitignore for test data

**File:** `test_data/.gitignore`

```
# Ignore large binary datasets
*.bin
*.frames

# Keep directory structure
!.gitkeep
```

### Step 7: Commit

```bash
git add tools/ test_data/.gitkeep test_data/.gitignore CMakeLists.txt
git commit -m "feat: add test dataset recording utility"
```

---

## Task 9: GitHub Actions CI/CD Pipeline

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/performance-regression.yml`

### Step 1: Write CI workflow

**File:** `.github/workflows/ci.yml`

```yaml
name: CI

on:
  push:
    branches: [ master, develop ]
  pull_request:
    branches: [ master ]

jobs:
  build-and-test:
    runs-on: windows-latest

    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup MSVC
      uses: microsoft/setup-msbuild@v2

    - name: Install CMake
      uses: lukka/get-cmake@latest

    - name: Install vcpkg
      uses: lukka/run-vcpkg@v11
      with:
        vcpkgGitCommitId: 'master'

    - name: Configure CMake
      run: |
        cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON

    - name: Build
      run: |
        cmake --build build --config Release -j

    - name: Run unit tests
      run: |
        cd build
        ctest -C Release --output-on-failure --label-regex unit

    - name: Run integration tests
      run: |
        cd build
        ctest -C Release --output-on-failure --label-regex integration

    - name: Upload test results
      if: always()
      uses: actions/upload-artifact@v4
      with:
        name: test-results
        path: build/Testing/Temporary/

    - name: Upload binaries
      uses: actions/upload-artifact@v4
      with:
        name: binaries-windows
        path: |
          build/bin/macroman_aimbot.exe
          build/bin/sunone-bench.exe
```

### Step 2: Write performance regression workflow

**File:** `.github/workflows/performance-regression.yml`

```yaml
name: Performance Regression

on:
  push:
    branches: [ master ]
  pull_request:
    branches: [ master ]
  schedule:
    # Run nightly at 2 AM UTC
    - cron: '0 2 * * *'

jobs:
  benchmark:
    runs-on: windows-latest

    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup MSVC
      uses: microsoft/setup-msbuild@v2

    - name: Install CMake
      uses: lukka/get-cmake@latest

    - name: Configure CMake
      run: |
        cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON

    - name: Build
      run: |
        cmake --build build --config Release -j

    - name: Download test dataset
      run: |
        # Download pre-recorded dataset from releases or artifact storage
        # For now, skip if dataset not available
        if (Test-Path "test_data/valorant_500frames.bin") {
          Write-Host "Dataset found"
        } else {
          Write-Host "Dataset not found - skipping benchmark"
          exit 0
        }

    - name: Run performance benchmark
      id: benchmark
      run: |
        ./build/bin/sunone-bench.exe `
          --dataset test_data/valorant_500frames.bin `
          --model models/valorant_yolov8_640.onnx `
          --threshold-avg-fps 120 `
          --threshold-p99-latency 15.0

    - name: Upload benchmark results
      if: always()
      uses: actions/upload-artifact@v4
      with:
        name: benchmark-results
        path: benchmark_results.json

    - name: Fail if performance regression
      if: steps.benchmark.outcome == 'failure'
      run: |
        Write-Error "Performance regression detected!"
        exit 1
```

### Step 3: Create GitHub directory structure

Run:
```bash
mkdir -p .github/workflows
```

### Step 4: Verify workflow syntax

Run:
```bash
# Install actionlint (optional)
# actionlint .github/workflows/*.yml
```

### Step 5: Test locally (optional - requires act)

Run:
```bash
# If 'act' is installed
# act -j build-and-test
```

### Step 6: Commit

```bash
git add .github/workflows/
git commit -m "ci: add GitHub Actions CI/CD pipeline with performance regression testing"
```

---

## Task 10: Performance Metrics Collection

**Files:**
- Create: `tests/benchmark/MetricsCollector.h`
- Create: `tests/benchmark/MetricsCollector.cpp`
- Modify: `tests/benchmark/BenchmarkRunner.cpp`

### Step 1: Write MetricsCollector header

**File:** `tests/benchmark/MetricsCollector.h`

```cpp
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <fstream>

struct PerformanceMetrics {
    // Latency metrics
    std::vector<float> frameLatencies;
    float avgLatency;
    float p50Latency;
    float p95Latency;
    float p99Latency;
    float maxLatency;

    // Throughput metrics
    float avgFPS;
    float minFPS;
    float maxFPS;

    // Resource metrics
    size_t peakMemoryMB;
    size_t vramUsageMB;

    // Detection metrics
    size_t totalDetections;
    float avgDetectionsPerFrame;

    // Safety metrics (from CRITICAL_TRAPS.md)
    size_t texturePoolStarved;
    size_t stalePredictionEvents;
    size_t deadmanSwitchTriggered;

    // Export to JSON
    void saveToJSON(const std::string& path) const;
};

class MetricsCollector {
public:
    void recordFrameLatency(float latency);
    void recordFPS(float fps);
    void recordMemory(size_t memoryMB);
    void recordVRAM(size_t vramMB);
    void recordDetections(size_t count);
    void recordSafetyMetric(const std::string& metricName, size_t value);

    PerformanceMetrics computeMetrics() const;

private:
    std::vector<float> latencies_;
    std::vector<float> fpsValues_;
    size_t peakMemory_{0};
    size_t vramUsage_{0};
    size_t totalDetections_{0};

    // Safety metrics
    size_t texturePoolStarved_{0};
    size_t stalePredictionEvents_{0};
    size_t deadmanSwitchTriggered_{0};

    float calculatePercentile(const std::vector<float>& data, float percentile) const;
};
```

### Step 2: Write MetricsCollector implementation

**File:** `tests/benchmark/MetricsCollector.cpp`

```cpp
#include "MetricsCollector.h"
#include <algorithm>
#include <numeric>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void MetricsCollector::recordFrameLatency(float latency) {
    latencies_.push_back(latency);
}

void MetricsCollector::recordFPS(float fps) {
    fpsValues_.push_back(fps);
}

void MetricsCollector::recordMemory(size_t memoryMB) {
    peakMemory_ = std::max(peakMemory_, memoryMB);
}

void MetricsCollector::recordVRAM(size_t vramMB) {
    vramUsage_ = vramMB;
}

void MetricsCollector::recordDetections(size_t count) {
    totalDetections_ += count;
}

void MetricsCollector::recordSafetyMetric(const std::string& metricName, size_t value) {
    if (metricName == "texturePoolStarved") {
        texturePoolStarved_ += value;
    } else if (metricName == "stalePredictionEvents") {
        stalePredictionEvents_ += value;
    } else if (metricName == "deadmanSwitchTriggered") {
        deadmanSwitchTriggered_ += value;
    }
}

PerformanceMetrics MetricsCollector::computeMetrics() const {
    PerformanceMetrics metrics;

    if (latencies_.empty()) return metrics;

    // Latency metrics
    metrics.frameLatencies = latencies_;
    metrics.avgLatency = std::accumulate(latencies_.begin(), latencies_.end(), 0.0f) / latencies_.size();
    metrics.p50Latency = calculatePercentile(latencies_, 0.50f);
    metrics.p95Latency = calculatePercentile(latencies_, 0.95f);
    metrics.p99Latency = calculatePercentile(latencies_, 0.99f);
    metrics.maxLatency = *std::max_element(latencies_.begin(), latencies_.end());

    // FPS metrics
    if (!fpsValues_.empty()) {
        metrics.avgFPS = std::accumulate(fpsValues_.begin(), fpsValues_.end(), 0.0f) / fpsValues_.size();
        metrics.minFPS = *std::min_element(fpsValues_.begin(), fpsValues_.end());
        metrics.maxFPS = *std::max_element(fpsValues_.begin(), fpsValues_.end());
    }

    // Resource metrics
    metrics.peakMemoryMB = peakMemory_;
    metrics.vramUsageMB = vramUsage_;

    // Detection metrics
    metrics.totalDetections = totalDetections_;
    metrics.avgDetectionsPerFrame = static_cast<float>(totalDetections_) / latencies_.size();

    // Safety metrics
    metrics.texturePoolStarved = texturePoolStarved_;
    metrics.stalePredictionEvents = stalePredictionEvents_;
    metrics.deadmanSwitchTriggered = deadmanSwitchTriggered_;

    return metrics;
}

float MetricsCollector::calculatePercentile(const std::vector<float>& data, float percentile) const {
    if (data.empty()) return 0.0f;

    std::vector<float> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(percentile * sorted.size());
    index = std::min(index, sorted.size() - 1);

    return sorted[index];
}

void PerformanceMetrics::saveToJSON(const std::string& path) const {
    json j;

    j["latency"] = {
        {"avg_ms", avgLatency},
        {"p50_ms", p50Latency},
        {"p95_ms", p95Latency},
        {"p99_ms", p99Latency},
        {"max_ms", maxLatency}
    };

    j["throughput"] = {
        {"avg_fps", avgFPS},
        {"min_fps", minFPS},
        {"max_fps", maxFPS}
    };

    j["resources"] = {
        {"peak_memory_mb", peakMemoryMB},
        {"vram_usage_mb", vramUsageMB}
    };

    j["detection"] = {
        {"total_detections", totalDetections},
        {"avg_per_frame", avgDetectionsPerFrame}
    };

    j["safety"] = {
        {"texture_pool_starved", texturePoolStarved},
        {"stale_prediction_events", stalePredictionEvents},
        {"deadman_switch_triggered", deadmanSwitchTriggered}
    };

    std::ofstream file(path);
    file << j.dump(4);  // Pretty print with 4-space indent
}
```

### Step 3: Update BenchmarkRunner to use MetricsCollector

**File:** `tests/benchmark/BenchmarkRunner.cpp` (modify run() method)

```cpp
BenchmarkResults BenchmarkRunner::run() {
    MetricsCollector collector;

    // ... existing code ...

    // Collect metrics during run
    for (size_t i = 0; i < pipeline.getProcessedFrameCount(); ++i) {
        collector.recordFrameLatency(pipeline.getFrameLatency(i));
        collector.recordFPS(pipeline.getInstantaneousFPS(i));
    }

    collector.recordMemory(pipeline.getPeakMemoryUsage());
    collector.recordVRAM(pipeline.getVRAMUsage());
    collector.recordDetections(pipeline.getTotalDetections());

    // Safety metrics
    auto safetyMetrics = pipeline.getSafetyMetrics();
    collector.recordSafetyMetric("texturePoolStarved", safetyMetrics.texturePoolStarved);
    collector.recordSafetyMetric("stalePredictionEvents", safetyMetrics.stalePredictionEvents);
    collector.recordSafetyMetric("deadmanSwitchTriggered", safetyMetrics.deadmanSwitchTriggered);

    // Compute and save metrics
    PerformanceMetrics perfMetrics = collector.computeMetrics();
    perfMetrics.saveToJSON("benchmark_results.json");

    // Convert to BenchmarkResults for return
    BenchmarkResults results;
    results.framesProcessed = pipeline.getProcessedFrameCount();
    results.avgFPS = perfMetrics.avgFPS;
    results.p95Latency = perfMetrics.p95Latency;
    results.p99Latency = perfMetrics.p99Latency;
    results.totalDetections = perfMetrics.totalDetections;
    results.latencies = perfMetrics.frameLatencies;

    return results;
}
```

### Step 4: Build and test

Run:
```bash
cmake --build build --config Release
```

Expected: Build succeeds

### Step 5: Commit

```bash
git add tests/benchmark/MetricsCollector.h tests/benchmark/MetricsCollector.cpp tests/benchmark/BenchmarkRunner.cpp
git commit -m "feat: add comprehensive performance metrics collection with JSON export"
```

---

## Task 11: Test Coverage Reporting (Optional)

**Files:**
- Create: `.github/workflows/coverage.yml`
- Modify: `CMakeLists.txt` (add coverage option)

### Step 1: Add coverage option to CMake

**File:** `CMakeLists.txt` (add after ENABLE_TESTS option)

```cmake
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    if(MSVC)
        # MSVC doesn't have built-in coverage like GCC/Clang
        # Would need OpenCppCoverage or similar tool
        message(WARNING "Coverage reporting on MSVC requires OpenCppCoverage")
    else()
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()
```

### Step 2: Create coverage workflow

**File:** `.github/workflows/coverage.yml`

```yaml
name: Code Coverage

on:
  push:
    branches: [ master ]
  pull_request:
    branches: [ master ]

jobs:
  coverage:
    runs-on: windows-latest

    steps:
    - name: Checkout code
      uses: actions/checkout@v4

    - name: Setup MSVC
      uses: microsoft/setup-msbuild@v2

    - name: Install OpenCppCoverage
      run: |
        choco install opencppcoverage -y
        echo "C:\Program Files\OpenCppCoverage" | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append

    - name: Configure CMake
      run: |
        cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON

    - name: Build
      run: |
        cmake --build build --config Debug

    - name: Run tests with coverage
      run: |
        OpenCppCoverage.exe `
          --sources extracted_modules `
          --excluded_sources tests `
          --export_type cobertura:coverage.xml `
          -- build\bin\Debug\unit_tests.exe

    - name: Upload coverage to Codecov
      uses: codecov/codecov-action@v4
      with:
        files: coverage.xml
        flags: unittests
        name: codecov-umbrella
```

### Step 3: Commit

```bash
git add .github/workflows/coverage.yml CMakeLists.txt
git commit -m "ci: add code coverage reporting workflow"
```

---

## Task 12: Documentation and Final Validation

**Files:**
- Create: `docs/TESTING.md`
- Modify: `README.md`

### Step 1: Write testing documentation

**File:** `docs/TESTING.md`

```markdown
# Testing Guide

## Overview

This project uses a comprehensive testing strategy with three tiers:
1. **Unit Tests** (Catch2) - Algorithm validation
2. **Integration Tests** (Catch2) - Full pipeline testing
3. **Benchmark Tests** (CLI tool) - Performance regression

---

## Running Tests

### All Tests
```bash
cd build
ctest -C Release --output-on-failure
```

### Unit Tests Only
```bash
ctest -C Release -R unit
```

### Integration Tests Only
```bash
ctest -C Release -R integration
```

### Specific Test Suite
```bash
./bin/unit_tests.exe "[kalman]"
./bin/unit_tests.exe "[nms]"
```

---

## Benchmarking

### Run Performance Benchmark
```bash
./bin/sunone-bench.exe \
  --dataset test_data/valorant_500frames.bin \
  --model models/valorant_yolov8_640.onnx \
  --threshold-avg-fps 120 \
  --threshold-p99-latency 15.0
```

### Record Test Dataset
```bash
# Start game, then run:
./bin/record_dataset.exe test_data/my_game_500frames.bin 500
```

---

## Performance Targets

| Metric | Target | P95 | Failure Indicator |
|--------|--------|-----|------------------|
| **Average FPS** | 120+ | 100+ | Model too complex |
| **P99 Latency** | <15ms | <20ms | GPU bottleneck |
| **Memory Usage** | <1GB | <1.2GB | Memory leak |
| **VRAM Usage** | <512MB | <600MB | Model too large |

---

## Safety Metrics (Critical Traps)

These metrics should ALWAYS be zero:

- `texturePoolStarved` - TexturePool starvation (Trap 1)
- `stalePredictionEvents` - Prediction extrapolation >50ms (Trap 2)
- `deadmanSwitchTriggered` - Input thread timeout (Trap 4)

If any safety metric > 0, investigate immediately.

---

## CI/CD Integration

Tests run automatically on:
- Every push to `master` or `develop`
- Every pull request
- Nightly at 2 AM UTC (performance regression)

See: `.github/workflows/ci.yml`

---

## Adding New Tests

### Unit Test
1. Create `tests/unit/test_<component>.cpp`
2. Add to `tests/unit/CMakeLists.txt`
3. Follow TDD: Write failing test first
4. Run: `ctest -R <test_name>`

### Integration Test
1. Create `tests/integration/test_<feature>.cpp`
2. Use fake implementations in `tests/integration/fakes/`
3. Add to `tests/integration/CMakeLists.txt`

---

## Test Data

Golden datasets stored in `test_data/`:
- `valorant_500frames.bin` - Valorant gameplay (500 frames)
- `cs2_500frames.bin` - CS2 gameplay (500 frames)

**Note:** Test datasets are NOT committed (ignored by .gitignore).
Download from releases or record your own.

---

## References

- Architecture Design: `docs/plans/2025-12-29-modern-aimbot-architecture-design.md`
- Critical Traps: `docs/CRITICAL_TRAPS.md`
- Main README: `README.md`
```

### Step 2: Update README.md with testing section

**File:** `README.md` (add Testing section)

```markdown
## Testing

### Run All Tests
```bash
cmake -B build -S . -DENABLE_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

### Performance Benchmark
```bash
./build/bin/sunone-bench.exe \
  --dataset test_data/valorant_500frames.bin \
  --model models/valorant_yolov8_640.onnx \
  --threshold-avg-fps 120 \
  --threshold-p99-latency 15.0
```

See: [Testing Guide](docs/TESTING.md)

### CI/CD Status
[![CI](https://github.com/yourusername/macroman_ai_aimbot/workflows/CI/badge.svg)](https://github.com/yourusername/macroman_ai_aimbot/actions)
[![Performance](https://github.com/yourusername/macroman_ai_aimbot/workflows/Performance%20Regression/badge.svg)](https://github.com/yourusername/macroman_ai_aimbot/actions)
```

### Step 3: Run all tests

Run:
```bash
cd build
ctest -C Release --output-on-failure
```

Expected: All tests pass

### Step 4: Verify benchmark tool works

Run:
```bash
./bin/sunone-bench.exe --help
```

Expected: Help message displayed

### Step 5: Commit

```bash
git add docs/TESTING.md README.md
git commit -m "docs: add comprehensive testing guide and update README"
```

---

## Final Validation Checklist

**Before marking Phase 7 complete, verify:**

- [ ] All unit tests pass on clean build
- [ ] Integration tests pass with fake implementations
- [ ] Benchmark tool compiles and shows help
- [ ] CI/CD workflows syntax is valid
- [ ] Test dataset recorder compiles
- [ ] Documentation is complete and accurate
- [ ] Safety metrics validation exists (Trap 1, 2, 4)
- [ ] Performance targets documented
- [ ] .gitignore excludes large test datasets

**Run final validation:**
```bash
# Clean build
rm -rf build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build --config Release -j

# Run all tests
cd build
ctest -C Release --output-on-failure

# Verify binaries
ls bin/unit_tests.exe
ls bin/integration_tests.exe
ls bin/sunone-bench.exe
ls bin/record_dataset.exe
```

**Expected:** All tests pass, all binaries exist

---

## Execution Handoff

Plan complete and saved to `docs/plans/2025-12-30-phase7-testing-benchmarking.md`.

**Two execution options:**

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** - Open new session with executing-plans, batch execution with checkpoints

**Which approach?**
