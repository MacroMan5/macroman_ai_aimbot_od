# Phase 3: Tracking & Prediction Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement multi-target tracking with Kalman filtering, data association, and target selection. Deliverable: System selects best target and outputs aim coordinates with <1ms tracking update latency.

**Architecture:** Structure-of-Arrays (SoA) TargetDatabase owned by Tracking thread. Greedy IoU-based data association matches detections to existing tracks. Kalman filter with constant velocity model predicts target positions. Target selection filters by FOV, sorts by distance, prioritizes by hitbox type.

**Tech Stack:** C++20, Eigen 3.4+ (linear algebra), Catch2 (testing), AVX2 intrinsics (SIMD optimization)

**Reference:** `docs/plans/2025-12-29-modern-aimbot-architecture-design.md` sections 5.2-5.4 (TargetDatabase SoA), 6.3 (Tracking Module), 9.1 (Performance Targets)

---

## Task P3-01: Math Types & State Structures

**Files:**
- Create: `src/core/entities/MathTypes.h`
- Create: `src/core/entities/KalmanState.h`
- Create: `tests/unit/test_math_types.cpp`

**Step 1: Write failing test for Vec2 operations**

Create `tests/unit/test_math_types.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/MathTypes.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 basic operations", "[math][vec2]") {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    SECTION("Addition") {
        Vec2 result = a + b;
        REQUIRE_THAT(result.x, WithinAbs(4.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(6.0f, 0.001f));
    }

    SECTION("Subtraction") {
        Vec2 result = a - b;
        REQUIRE_THAT(result.x, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Scalar multiplication") {
        Vec2 result = a * 2.0f;
        REQUIRE_THAT(result.x, WithinAbs(6.0f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(8.0f, 0.001f));
    }

    SECTION("Scalar division") {
        Vec2 result = a / 2.0f;
        REQUIRE_THAT(result.x, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(result.y, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Length") {
        float len = a.length();
        REQUIRE_THAT(len, WithinAbs(5.0f, 0.001f));  // sqrt(3^2 + 4^2) = 5
    }

    SECTION("Distance") {
        float dist = Vec2::distance(a, b);
        REQUIRE_THAT(dist, WithinAbs(2.828f, 0.01f));  // sqrt((3-1)^2 + (4-2)^2)
    }

    SECTION("Normalized") {
        Vec2 norm = a.normalized();
        REQUIRE_THAT(norm.x, WithinAbs(0.6f, 0.001f));
        REQUIRE_THAT(norm.y, WithinAbs(0.8f, 0.001f));
        REQUIRE_THAT(norm.length(), WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("TargetID comparison", "[math][targetid]") {
    TargetID id1{42};
    TargetID id2{42};
    TargetID id3{99};

    REQUIRE(id1 == id2);
    REQUIRE(id1 != id3);
    REQUIRE(id1.value == 42);
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_math_types -C Debug --output-on-failure`
Expected: FAIL with "MathTypes.h: No such file or directory"

**Step 3: Implement MathTypes.h**

Create `src/core/entities/MathTypes.h`:

```cpp
#pragma once

#include <cmath>
#include <cstdint>

namespace macroman {

/**
 * @brief 2D vector for positions and velocities
 *
 * Aligned to 8 bytes for potential SIMD usage.
 */
struct alignas(8) Vec2 {
    float x{0.0f};
    float y{0.0f};

    // Arithmetic operators
    Vec2 operator+(const Vec2& other) const noexcept {
        return {x + other.x, y + other.y};
    }

    Vec2 operator-(const Vec2& other) const noexcept {
        return {x - other.x, y - other.y};
    }

    Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    Vec2 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar};
    }

    Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    // Magnitude
    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    [[nodiscard]] float lengthSquared() const noexcept {
        return x * x + y * y;
    }

    // Normalization
    [[nodiscard]] Vec2 normalized() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }

    // Static utilities
    [[nodiscard]] static float distance(const Vec2& a, const Vec2& b) noexcept {
        return (b - a).length();
    }

    [[nodiscard]] static float distanceSquared(const Vec2& a, const Vec2& b) noexcept {
        return (b - a).lengthSquared();
    }

    [[nodiscard]] static float dot(const Vec2& a, const Vec2& b) noexcept {
        return a.x * b.x + a.y * b.y;
    }
};

/**
 * @brief Unique target identifier (monotonically increasing)
 */
struct TargetID {
    uint64_t value{0};

    bool operator==(const TargetID& other) const noexcept {
        return value == other.value;
    }

    bool operator!=(const TargetID& other) const noexcept {
        return value != other.value;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return value != 0;
    }

    // For std::unordered_map
    struct Hash {
        size_t operator()(const TargetID& id) const noexcept {
            return std::hash<uint64_t>{}(id.value);
        }
    };
};

} // namespace macroman
```

**Step 4: Implement KalmanState.h**

Create `src/core/entities/KalmanState.h`:

```cpp
#pragma once

#include <array>

namespace macroman {

/**
 * @brief Kalman filter state (constant velocity model)
 *
 * STATE VECTOR: [x, y, vx, vy]
 * - x, y: Position in pixels
 * - vx, vy: Velocity in pixels/second
 *
 * COVARIANCE MATRIX P (4x4):
 * - Represented as flat array for SoA storage
 */
struct KalmanState {
    float x{0.0f};   // Position X (pixels)
    float y{0.0f};   // Position Y (pixels)
    float vx{0.0f};  // Velocity X (pixels/second)
    float vy{0.0f};  // Velocity Y (pixels/second)

    // Flat covariance matrix (4x4 = 16 elements)
    // Initialized to high uncertainty
    std::array<float, 16> covariance{};

    KalmanState() {
        covariance.fill(0.0f);
        // Identity * 1000.0f
        covariance[0] = 1000.0f;
        covariance[5] = 1000.0f;
        covariance[10] = 1000.0f;
        covariance[15] = 1000.0f;
    }
};

} // namespace macroman
```

**Step 5: Add to CMake**

Modify `src/core/CMakeLists.txt` (add to existing):

```cmake
# Core entities (header-only)
target_sources(core INTERFACE
    entities/MathTypes.h
    entities/KalmanState.h
    entities/Detection.h  # From Phase 1
    entities/Frame.h      # From Phase 2
)
```

**Step 6: Run test to verify it passes**

Run: `ctest --test-dir build -R test_math_types -C Debug --output-on-failure`
Expected: PASS (all sections)

**Step 7: Commit**

```bash
git add src/core/entities/MathTypes.h src/core/entities/KalmanState.h tests/unit/test_math_types.cpp src/core/CMakeLists.txt
git commit -m "feat(core): add math types and Kalman state structure

- Vec2: Added division and scalar compound operators
- KalmanState: POD structure for SoA storage
- TargetID: Unique 64-bit identifier
- Unit tests: Verified all Vec2 operations"
```

---

## Task P3-02: DetectionBatch Structure

**Files:**
- Create: `src/core/entities/DetectionBatch.h`
- Create: `tests/unit/test_detection_batch.cpp`

**Step 1: Write failing test for DetectionBatch**

Create `tests/unit/test_detection_batch.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/entities/DetectionBatch.h"

using namespace macroman;

TEST_CASE("DetectionBatch construction", "[entities][detection_batch]") {
    DetectionBatch batch;

    REQUIRE(batch.observations.empty());
    REQUIRE(batch.frameSequence == 0);
    REQUIRE(batch.captureTimeNs == 0);
}

TEST_CASE("DetectionBatch add detections", "[entities][detection_batch]") {
    DetectionBatch batch;
    batch.frameSequence = 42;
    batch.captureTimeNs = 1000000000;  // 1 second in nanoseconds

    Detection det1{{10.0f, 20.0f, 50.0f, 60.0f}, 0.9f, 0, HitboxType::Head};
    Detection det2{{100.0f, 200.0f, 50.0f, 60.0f}, 0.85f, 1, HitboxType::Chest};

    batch.observations.push_back(det1);
    batch.observations.push_back(det2);

    REQUIRE(batch.observations.size() == 2);
    REQUIRE(batch.observations[0].confidence == 0.9f);
    REQUIRE(batch.observations[1].hitbox == HitboxType::Chest);
}

TEST_CASE("DetectionBatch capacity limit", "[entities][detection_batch]") {
    DetectionBatch batch;

    // Fill to capacity
    for (size_t i = 0; i < DetectionBatch::MAX_DETECTIONS; ++i) {
        Detection det{{0.0f, 0.0f, 1.0f, 1.0f}, 0.9f, 0, HitboxType::Body};
        batch.observations.push_back(det);
    }

    REQUIRE(batch.observations.size() == DetectionBatch::MAX_DETECTIONS);
    REQUIRE(batch.observations.capacity() == DetectionBatch::MAX_DETECTIONS);
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_detection_batch -C Debug --output-on-failure`
Expected: FAIL with "DetectionBatch.h: No such file or directory"

**Step 3: Implement DetectionBatch.h**

Create `src/core/entities/DetectionBatch.h`:

```cpp
#pragma once

#include <vector>
#include <cstdint>
#include "Detection.h"

namespace macroman {

/**
 * @brief Batch of detections from a single frame
 *
 * CRITICAL: Using std::vector with reserved capacity instead of FixedCapacityVector
 * for MVP simplicity. Pre-allocate to MAX_DETECTIONS to avoid reallocation in hot path.
 *
 * TODO Phase 8: Replace with FixedCapacityVector for zero-allocation guarantee.
 *
 * Data Flow:
 *   Detection Thread → creates DetectionBatch → pushes to LatestFrameQueue
 *   Tracking Thread → pops DetectionBatch → processes → deletes
 */
struct DetectionBatch {
    static constexpr size_t MAX_DETECTIONS = 64;

    std::vector<Detection> observations;  // Pre-allocated to MAX_DETECTIONS
    uint64_t frameSequence{0};            // Corresponds to Frame::frameSequence
    int64_t captureTimeNs{0};             // Timestamp from Frame::captureTimeNs

    DetectionBatch() {
        observations.reserve(MAX_DETECTIONS);  // Pre-allocate to avoid reallocation
    }

    // Move-only (no copies in hot path)
    DetectionBatch(const DetectionBatch&) = delete;
    DetectionBatch& operator=(const DetectionBatch&) = delete;
    DetectionBatch(DetectionBatch&&) noexcept = default;
    DetectionBatch& operator=(DetectionBatch&&) noexcept = default;

    [[nodiscard]] bool isEmpty() const noexcept {
        return observations.empty();
    }
};

} // namespace macroman
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R test_detection_batch -C Debug --output-on-failure`
Expected: PASS (all 3 test cases)

**Step 5: Commit**

```bash
git add src/core/entities/DetectionBatch.h tests/unit/test_detection_batch.cpp
git commit -m "feat(core): add DetectionBatch structure

- Pre-allocated std::vector with MAX_DETECTIONS capacity
- Move-only semantics (no copies in hot path)
- Carries frame sequence and capture timestamp
- Unit tests: construction, add detections, capacity limit"
```

---

## Task P3-03: TargetDatabase Structure (SoA Layout with Prediction)

**Files:**
- Create: `src/core/entities/TargetDatabase.h`
- Create: `tests/unit/test_target_database.cpp`

**Step 1: Write failing test for TargetDatabase add/remove**

Create `tests/unit/test_target_database.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/TargetDatabase.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("TargetDatabase construction", "[entities][target_database]") {
    TargetDatabase db;

    REQUIRE(db.count == 0);
    REQUIRE(db.getCount() == 0);
    REQUIRE(db.isEmpty());
    REQUIRE_FALSE(db.isFull());
}

TEST_CASE("TargetDatabase add target", "[entities][target_database]") {
    TargetDatabase db;

    TargetID id{1};
    Vec2 pos{100.0f, 200.0f};
    Vec2 vel{5.0f, -3.0f};
    BBox bbox{95.0f, 195.0f, 10.0f, 10.0f};
    float confidence = 0.9f;
    HitboxType hitbox = HitboxType::Head;
    int64_t timestamp = 1000000000;

    size_t index = db.addTarget(id, pos, vel, bbox, confidence, hitbox, timestamp);

    REQUIRE(index == 0);
    REQUIRE(db.count == 1);
    REQUIRE(db.ids[0] == id);
    REQUIRE_THAT(db.positions[0].x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(db.velocities[0].y, WithinAbs(-3.0f, 0.001f));
    REQUIRE(db.hitboxTypes[0] == HitboxType::Head);
}

TEST_CASE("TargetDatabase prediction update", "[entities][target_database]") {
    TargetDatabase db;
    db.addTarget(TargetID{1}, {100.0f, 100.0f}, {100.0f, 0.0f}, {}, 0.9f, HitboxType::Body, 0);

    // Update prediction by 0.1s: 100 + 100 * 0.1 = 110
    db.updatePredictions(0.1f);

    REQUIRE_THAT(db.positions[0].x, WithinAbs(110.0f, 0.001f));
    REQUIRE_THAT(db.positions[0].y, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("TargetDatabase remove target", "[entities][target_database]") {
    TargetDatabase db;

    TargetID id1{1};
    TargetID id2{2};
    TargetID id3{3};

    db.addTarget(id1, {100.0f, 100.0f}, {}, {}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {}, 0.9f, HitboxType::Chest, 0);
    db.addTarget(id3, {300.0f, 300.0f}, {}, {}, 0.9f, HitboxType::Body, 0);

    REQUIRE(db.count == 3);

    // Remove middle target (id2 at index 1)
    bool removed = db.removeTarget(1);

    REQUIRE(removed);
    REQUIRE(db.count == 2);

    // id3 should now be at index 1 (swap-erase)
    REQUIRE(db.ids[1] == id3);
    REQUIRE_THAT(db.positions[1].x, WithinAbs(300.0f, 0.001f));
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_target_database -C Debug --output-on-failure`
Expected: FAIL with "TargetDatabase.h: No such file or directory"

**Step 3: Implement TargetDatabase.h**

Create `src/core/entities/TargetDatabase.h`:

```cpp
#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <cassert>
#include "MathTypes.h"
#include "KalmanState.h"
#include "Detection.h"

namespace macroman {

/**
 * @brief Structure-of-Arrays (SoA) database for target tracking
 *
 * ARCHITECTURE:
 * - Owned exclusively by Tracking Thread (no shared state, no locks)
 * - SoA layout for cache efficiency (iterate positions without loading all fields)
 * - Pre-allocated arrays (no allocations in hot path)
 * - Swap-erase for removal (O(1), preserves cache locality)
 */
struct alignas(32) TargetDatabase {
    static constexpr size_t MAX_TARGETS = 64;

    // Parallel arrays (SoA layout)
    alignas(32) std::array<TargetID, MAX_TARGETS> ids{};
    alignas(32) std::array<Vec2, MAX_TARGETS> positions{};
    alignas(32) std::array<Vec2, MAX_TARGETS> velocities{};
    alignas(32) std::array<BBox, MAX_TARGETS> bboxes{};
    alignas(32) std::array<float, MAX_TARGETS> confidences{};
    alignas(32) std::array<HitboxType, MAX_TARGETS> hitboxTypes{};
    alignas(32) std::array<int64_t, MAX_TARGETS> lastSeenNs{};  // Timestamp in nanoseconds
    alignas(32) std::array<KalmanState, MAX_TARGETS> kalmanStates{};

    size_t count{0};  // Active targets [0, MAX_TARGETS]

    /**
     * @brief Add new target to database
     *
     * @return Index of added target, or MAX_TARGETS if database is full
     */
    size_t addTarget(
        TargetID id,
        Vec2 position,
        Vec2 velocity,
        BBox bbox,
        float confidence,
        HitboxType hitbox,
        int64_t timestamp
    ) noexcept {
        if (count >= MAX_TARGETS) {
            return MAX_TARGETS;  // Database full
        }

        size_t index = count++;
        ids[index] = id;
        positions[index] = position;
        velocities[index] = velocity;
        bboxes[index] = bbox;
        confidences[index] = confidence;
        hitboxTypes[index] = hitbox;
        lastSeenNs[index] = timestamp;
        kalmanStates[index] = KalmanState(); // Reset state

        return index;
    }

    /**
     * @brief Find target index by ID
     */
    [[nodiscard]] std::optional<size_t> findTarget(TargetID id) const noexcept {
        for (size_t i = 0; i < count; ++i) {
            if (ids[i] == id) {
                return i;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Remove target at index using swap-erase
     * @return true if removed
     */
    bool removeTarget(size_t index) noexcept {
        if (index >= count) return false;

        size_t lastIndex = count - 1;
        if (index != lastIndex) {
            ids[index] = ids[lastIndex];
            positions[index] = positions[lastIndex];
            velocities[index] = velocities[lastIndex];
            bboxes[index] = bboxes[lastIndex];
            confidences[index] = confidences[lastIndex];
            hitboxTypes[index] = hitboxTypes[lastIndex];
            lastSeenNs[index] = lastSeenNs[lastIndex];
            kalmanStates[index] = kalmanStates[lastIndex];
        }

        --count;
        return true;
    }

    /**
     * @brief Scalar prediction update for all targets (SoA efficiency)
     *
     * TODO Phase 8: Optimize with AVX2 intrinsics
     */
    void updatePredictions(float dt) noexcept {
        for (size_t i = 0; i < count; ++i) {
            positions[i].x += velocities[i].x * dt;
            positions[i].y += velocities[i].y * dt;
        }
    }

    [[nodiscard]] size_t getCount() const noexcept { return count; }
    [[nodiscard]] bool isEmpty() const noexcept { return count == 0; }
    [[nodiscard]] bool isFull() const noexcept { return count >= MAX_TARGETS; }
};

} // namespace macroman
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R test_target_database -C Debug --output-on-failure`
Expected: PASS (all 4 test cases)

**Step 5: Commit**

```bash
git add src/core/entities/TargetDatabase.h tests/unit/test_target_database.cpp
git commit -m "feat(core): implement TargetDatabase SoA with prediction

- Added kalmanStates to parallel arrays
- Implemented updatePredictions() for bulk position extrapolation
- Integrated swap-erase for all SoA fields
- Unit tests: Construction, add, remove, and prediction logic"
```

---

## Task P3-04: Data Association Algorithm (Greedy IoU Matching)

**Files:**
- Create: `src/tracking/DataAssociation.h`
- Create: `src/tracking/DataAssociation.cpp`
- Create: `tests/unit/test_data_association.cpp`

**Step 1: Write failing test for IoU calculation**

Create `tests/unit/test_data_association.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/DataAssociation.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("IoU calculation", "[tracking][data_association]") {
    SECTION("Perfect overlap (IoU = 1.0)") {
        BBox a{10.0f, 20.0f, 50.0f, 60.0f};
        BBox b{10.0f, 20.0f, 50.0f, 60.0f};

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(1.0f, 0.001f));
    }

    SECTION("No overlap (IoU = 0.0)") {
        BBox a{10.0f, 20.0f, 50.0f, 60.0f};
        BBox b{100.0f, 200.0f, 50.0f, 60.0f};

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Partial overlap") {
        BBox a{0.0f, 0.0f, 10.0f, 10.0f};    // Area = 100
        BBox b{5.0f, 5.0f, 10.0f, 10.0f};    // Area = 100
        // Intersection: (5,5) to (10,10) = 5x5 = 25
        // Union: 100 + 100 - 25 = 175
        // IoU: 25 / 175 ≈ 0.143

        float iou = DataAssociation::computeIoU(a, b);
        REQUIRE_THAT(iou, WithinAbs(0.143f, 0.01f));
    }
}

TEST_CASE("Data association greedy matching", "[tracking][data_association]") {
    TargetDatabase db;

    // Add 2 existing targets
    TargetID id1{1};
    TargetID id2{2};
    db.addTarget(id1, {100.0f, 100.0f}, {}, {90.0f, 90.0f, 20.0f, 20.0f}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {190.0f, 190.0f, 20.0f, 20.0f}, 0.85f, HitboxType::Chest, 0);

    // Create 3 new detections
    std::vector<Detection> detections;
    detections.push_back({{92.0f, 92.0f, 20.0f, 20.0f}, 0.92f, 0, HitboxType::Head});      // Matches id1 (high IoU)
    detections.push_back({{195.0f, 195.0f, 20.0f, 20.0f}, 0.88f, 1, HitboxType::Chest});   // Matches id2 (high IoU)
    detections.push_back({{300.0f, 300.0f, 20.0f, 20.0f}, 0.80f, 2, HitboxType::Body});    // New target (low IoU)

    DataAssociation::AssociationResult result = DataAssociation::matchDetections(db, detections, 0.3f);

    SECTION("Matched detections") {
        REQUIRE(result.matches.size() == 2);

        // Detection 0 should match target index 0 (id1)
        REQUIRE(result.matches[0].detectionIndex == 0);
        REQUIRE(result.matches[0].targetIndex == 0);

        // Detection 1 should match target index 1 (id2)
        REQUIRE(result.matches[1].detectionIndex == 1);
        REQUIRE(result.matches[1].targetIndex == 1);
    }

    SECTION("Unmatched detections (new targets)") {
        REQUIRE(result.unmatchedDetections.size() == 1);
        REQUIRE(result.unmatchedDetections[0] == 2);  // Detection index 2 (new target)
    }

    SECTION("Unmatched targets (lost tracks)") {
        REQUIRE(result.unmatchedTargets.empty());  // All targets were matched
    }
}

TEST_CASE("Data association with lost targets", "[tracking][data_association]") {
    TargetDatabase db;

    TargetID id1{1};
    TargetID id2{2};
    db.addTarget(id1, {100.0f, 100.0f}, {}, {90.0f, 90.0f, 20.0f, 20.0f}, 0.9f, HitboxType::Head, 0);
    db.addTarget(id2, {200.0f, 200.0f}, {}, {190.0f, 190.0f, 20.0f, 20.0f}, 0.85f, HitboxType::Chest, 0);

    // Only 1 detection (id2 disappears)
    std::vector<Detection> detections;
    detections.push_back({{92.0f, 92.0f, 20.0f, 20.0f}, 0.92f, 0, HitboxType::Head});

    DataAssociation::AssociationResult result = DataAssociation::matchDetections(db, detections, 0.3f);

    REQUIRE(result.matches.size() == 1);
    REQUIRE(result.unmatchedDetections.empty());
    REQUIRE(result.unmatchedTargets.size() == 1);
    REQUIRE(result.unmatchedTargets[0] == 1);  // Target index 1 (id2) lost
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_data_association -C Debug --output-on-failure`
Expected: FAIL with "DataAssociation.h: No such file or directory"

**Step 3: Implement DataAssociation.h**

Create `src/tracking/DataAssociation.h`:

```cpp
#pragma once

#include <vector>
#include "core/entities/TargetDatabase.h"
#include "core/entities/Detection.h"

namespace macroman {

/**
 * @brief Data association algorithms for matching detections to existing targets
 *
 * ALGORITHM: Greedy IoU matching (simple, fast, sufficient for MVP)
 */
class DataAssociation {
public:
    struct Match {
        size_t detectionIndex;  // Index in detections vector
        size_t targetIndex;     // Index in TargetDatabase
        float iou;              // Intersection-over-Union score
    };

    struct AssociationResult {
        std::vector<Match> matches;                 // Successfully matched pairs
        std::vector<size_t> unmatchedDetections;    // New targets to add
        std::vector<size_t> unmatchedTargets;       // Lost targets (to be checked for grace period)
    };

    [[nodiscard]] static float computeIoU(const BBox& a, const BBox& b) noexcept;

    [[nodiscard]] static AssociationResult matchDetections(
        const TargetDatabase& targets,
        const std::vector<Detection>& detections,
        float iouThreshold = 0.3f
    ) noexcept;
};

} // namespace macroman
```

**Step 4: Implement DataAssociation.cpp**

Create `src/tracking/DataAssociation.cpp`:

```cpp
#include "DataAssociation.h"
#include <algorithm>
#include <limits>

namespace macroman {

float DataAssociation::computeIoU(const BBox& a, const BBox& b) noexcept {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);

    if (x2 <= x1 || y2 <= y1) return 0.0f;

    float intersectionArea = (x2 - x1) * (y2 - y1);
    float unionArea = a.area() + b.area() - intersectionArea;

    if (unionArea < 1e-6f) return 0.0f;
    return intersectionArea / unionArea;
}

DataAssociation::AssociationResult DataAssociation::matchDetections(
    const TargetDatabase& targets,
    const std::vector<Detection>& detections,
    float iouThreshold
) noexcept {
    AssociationResult result;
    const size_t numTargets = targets.count;
    const size_t numDetections = detections.size();

    std::vector<bool> detectionMatched(numDetections, false);
    std::vector<bool> targetMatched(numTargets, false);

    while (true) {
        float bestIoU = iouThreshold;
        size_t bestDetectionIdx = SIZE_MAX;
        size_t bestTargetIdx = SIZE_MAX;

        for (size_t t = 0; t < numTargets; ++t) {
            if (targetMatched[t]) continue;
            for (size_t d = 0; d < numDetections; ++d) {
                if (detectionMatched[d]) continue;
                float iou = computeIoU(targets.bboxes[t], detections[d].bbox);
                if (iou > bestIoU) {
                    bestIoU = iou;
                    bestDetectionIdx = d;
                    bestTargetIdx = t;
                }
            }
        }

        if (bestDetectionIdx == SIZE_MAX) break;

        result.matches.push_back({bestDetectionIdx, bestTargetIdx, bestIoU});
        detectionMatched[bestDetectionIdx] = true;
        targetMatched[bestTargetIdx] = true;
    }

    for (size_t d = 0; d < numDetections; ++d) {
        if (!detectionMatched[d]) result.unmatchedDetections.push_back(d);
    }
    for (size_t t = 0; t < numTargets; ++t) {
        if (!targetMatched[t]) result.unmatchedTargets.push_back(t);
    }

    return result;
}

} // namespace macroman
```

**Step 5: Add to CMake**

Create `src/tracking/CMakeLists.txt`:

```cmake
add_library(tracking STATIC
    DataAssociation.cpp
)

target_include_directories(tracking PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(tracking PUBLIC
    core
)
```

Modify `src/CMakeLists.txt` (add after other subdirectories):

```cmake
add_subdirectory(tracking)
```

**Step 6: Run test to verify it passes**

Run: `ctest --test-dir build -R test_data_association -C Debug --output-on-failure`
Expected: PASS

**Step 7: Commit**

```bash
git add src/tracking/DataAssociation.h src/tracking/DataAssociation.cpp tests/unit/test_data_association.cpp src/tracking/CMakeLists.txt
git commit -m "feat(tracking): implement greedy IoU data association

- matchDetections algorithm for matching detections to existing tracks
- Intersection-over-Union utility
- Unit tests for overlap scenarios and track loss"
```

---

## Task P3-05: Kalman Predictor (Stateless SoA Support)

**Files:**
- Create: `src/tracking/KalmanPredictor.h`
- Create: `src/tracking/KalmanPredictor.cpp`
- Create: `tests/unit/test_kalman_predictor.cpp`

**Step 1: Write failing test for Kalman prediction**

Create `tests/unit/test_kalman_predictor.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/KalmanPredictor.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("Kalman stateless update", "[tracking][kalman]") {
    KalmanPredictor predictor;
    KalmanState state; // Initialized to zero pos, zero vel, high uncertainty

    // Observation at (100, 200)
    predictor.update({100.0f, 200.0f}, 0.016f, state);

    REQUIRE_THAT(state.x, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(state.y, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Kalman prediction with constant velocity", "[tracking][kalman]") {
    KalmanPredictor predictor;
    KalmanState state;

    // Simulate movement: (100, 100) -> (110, 100) at 60 FPS (0.016s)
    predictor.update({100.0f, 100.0f}, 0.016f, state);
    predictor.update({110.0f, 100.0f}, 0.016f, state);

    // Expected velocity ≈ 10 / 0.016 = 625 px/s
    REQUIRE(state.vx > 500.0f);

    // Predict 3 frames ahead (0.048s)
    // 110 + (625 * 0.048) ≈ 140
    Vec2 predicted = predictor.predict(0.048f, state);

    REQUIRE_THAT(predicted.x, WithinAbs(140.0f, 5.0f));
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_kalman_predictor -C Debug --output-on-failure`
Expected: FAIL

**Step 3: Implement KalmanPredictor.h**

Create `src/tracking/KalmanPredictor.h`:

```cpp
#pragma once

#include <Eigen/Dense>
#include "core/entities/MathTypes.h"
#include "core/entities/KalmanState.h"

namespace macroman {

/**
 * @brief Stateless Kalman filter for target prediction
 *
 * This implementation is stateless to support SoA architectures.
 * It takes a reference to a KalmanState POD which stores the
 * state vector and covariance matrix.
 */
class KalmanPredictor {
public:
    KalmanPredictor(float processNoise = 10.0f, float measurementNoise = 5.0f);

    /**
     * @brief Update state with a new observation
     * @param observation Measured position [x, y]
     * @param dt Time since last update
     * @param state State POD to update (stored in TargetDatabase)
     */
    void update(Vec2 observation, float dt, KalmanState& state) const noexcept;

    /**
     * @brief Predict position ahead in time
     * @param dt Time to predict ahead
     * @param state Current state POD
     */
    [[nodiscard]] Vec2 predict(float dt, const KalmanState& state) const noexcept;

private:
    float q_val_; // Process noise scaling
    float r_val_; // Measurement noise scaling

    // Internal Eigen mapping helpers
    static void toEigen(const KalmanState& state, Eigen::Vector4f& x, Eigen::Matrix4f& P) noexcept;
    static void fromEigen(const Eigen::Vector4f& x, const Eigen::Matrix4f& P, KalmanState& state) noexcept;
};

} // namespace macroman
```

**Step 4: Implement KalmanPredictor.cpp**

Create `src/tracking/KalmanPredictor.cpp`:

```cpp
#include "KalmanPredictor.h"

namespace macroman {

KalmanPredictor::KalmanPredictor(float processNoise, float measurementNoise)
    : q_val_(processNoise), r_val_(measurementNoise) {}

void KalmanPredictor::toEigen(const KalmanState& state, Eigen::Vector4f& x, Eigen::Matrix4f& P) noexcept {
    x << state.x, state.y, state.vx, state.vy;
    for (int i = 0; i < 16; ++i) P(i) = state.covariance[i];
}

void KalmanPredictor::fromEigen(const Eigen::Vector4f& x, const Eigen::Matrix4f& P, KalmanState& state) noexcept {
    state.x = x(0); state.y = x(1); state.vx = x(2); state.vy = x(3);
    for (int i = 0; i < 16; ++i) state.covariance[i] = P(i);
}

void KalmanPredictor::update(Vec2 observation, float dt, KalmanState& state) const noexcept {
    Eigen::Vector4f x; Eigen::Matrix4f P;
    toEigen(state, x, P);

    // 1. Prediction step
    Eigen::Matrix4f F;
    F << 1, 0, dt, 0,
         0, 1, 0,  dt,
         0, 0, 1,  0,
         0, 0, 0,  1;

    Eigen::Matrix4f Q = Eigen::Matrix4f::Identity() * q_val_;
    x = F * x;
    P = F * P * F.transpose() + Q;

    // 2. Correction step
    Eigen::Matrix<float, 2, 4> H;
    H << 1, 0, 0, 0,
         0, 1, 0, 0;

    Eigen::Matrix2f R = Eigen::Matrix2f::Identity() * r_val_;
    Eigen::Vector2f z(observation.x, observation.y);
    Eigen::Vector2f y = z - H * x;
    Eigen::Matrix2f S = H * P * H.transpose() + R;
    Eigen::Matrix<float, 4, 2> K = P * H.transpose() * S.inverse();

    x = x + K * y;
    P = (Eigen::Matrix4f::Identity() - K * H) * P;

    fromEigen(x, P, state);
}

Vec2 KalmanPredictor::predict(float dt, const KalmanState& state) const noexcept {
    return {
        state.x + state.vx * dt,
        state.y + state.vy * dt
    };
}

} // namespace macroman
```

**Step 5: Add Eigen to CMakeLists.txt**

Modify `CMakeLists.txt` root (add after other dependencies):

```cmake
# Eigen (header-only linear algebra library)
find_package(Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0
        GIT_SHALLOW TRUE
    )
    set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Eigen)
endif()
```

Modify `src/tracking/CMakeLists.txt`:

```cmake
target_link_libraries(tracking PUBLIC
    core
    Eigen3::Eigen
)
```

**Step 6: Run test to verify it passes**

Run: `ctest --test-dir build -R test_kalman_predictor -C Debug --output-on-failure`
Expected: PASS

**Step 7: Commit**

```bash
git add src/tracking/KalmanPredictor.h src/tracking/KalmanPredictor.cpp tests/unit/test_kalman_predictor.cpp CMakeLists.txt src/tracking/CMakeLists.txt
git commit -m "feat(tracking): implement stateless Kalman predictor for SoA

- Stateless update() takes KalmanState POD from TargetDatabase
- Uses Eigen 3.4 for matrix math
- Constant velocity model [x, y, vx, vy]
- Unit tests: single update and constant velocity prediction"
```

---

## Task P3-06: Target Selection Algorithm

**Files:**
- Create: `src/tracking/TargetSelector.h`
- Create: `src/tracking/TargetSelector.cpp`
- Create: `tests/unit/test_target_selector.cpp`

**Step 1: Write failing test for target selection**

Create `tests/unit/test_target_selector.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/TargetSelector.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("TargetSelector FOV filtering", "[tracking][target_selector]") {
    TargetDatabase db;

    // Crosshair at center (500, 400)
    Vec2 crosshair{500.0f, 400.0f};
    float fov = 100.0f;  // 100 pixel radius

    // Add 3 targets
    db.addTarget(TargetID{1}, {510.0f, 410.0f}, {}, {}, 0.9f, HitboxType::Head, 0);    // Inside FOV (distance ≈ 14)
    db.addTarget(TargetID{2}, {700.0f, 400.0f}, {}, {}, 0.85f, HitboxType::Chest, 0);  // Outside FOV (distance = 200)
    db.addTarget(TargetID{3}, {520.0f, 420.0f}, {}, {}, 0.88f, HitboxType::Body, 0);   // Inside FOV (distance ≈ 28)

    std::vector<size_t> insideFOV = TargetSelector::filterByFOV(db, crosshair, fov);

    REQUIRE(insideFOV.size() == 2);
    REQUIRE(insideFOV[0] == 0);  // Target index 0 (id1)
    REQUIRE(insideFOV[1] == 2);  // Target index 2 (id3)
}

TEST_CASE("TargetSelector distance sorting", "[tracking][target_selector]") {
    TargetDatabase db;

    Vec2 crosshair{100.0f, 100.0f};

    // Add targets at varying distances
    db.addTarget(TargetID{1}, {150.0f, 150.0f}, {}, {}, 0.9f, HitboxType::Head, 0);   // Distance ≈ 70.7
    db.addTarget(TargetID{2}, {110.0f, 110.0f}, {}, {}, 0.85f, HitboxType::Chest, 0); // Distance ≈ 14.1 (closest)
    db.addTarget(TargetID{3}, {130.0f, 130.0f}, {}, {}, 0.88f, HitboxType::Body, 0);  // Distance ≈ 42.4

    std::vector<size_t> indices = {0, 1, 2};
    TargetSelector::sortByDistance(db, indices, crosshair);

    REQUIRE(indices[0] == 1);  // Closest
    REQUIRE(indices[1] == 2);
    REQUIRE(indices[2] == 0);  // Farthest
}

TEST_CASE("TargetSelector hitbox prioritization", "[tracking][target_selector]") {
    TargetDatabase db;

    Vec2 crosshair{100.0f, 100.0f};

    // Add 3 targets at SAME distance (110, 110) but different hitboxes
    db.addTarget(TargetID{1}, {110.0f, 110.0f}, {}, {}, 0.9f, HitboxType::Body, 0);
    db.addTarget(TargetID{2}, {110.0f, 110.0f}, {}, {}, 0.85f, HitboxType::Head, 0);
    db.addTarget(TargetID{3}, {110.0f, 110.0f}, {}, {}, 0.88f, HitboxType::Chest, 0);

    std::vector<size_t> indices = {0, 1, 2};
    TargetSelector::sortByDistance(db, indices, crosshair);
    TargetSelector::prioritizeByHitbox(db, indices);

    // After prioritization: Head > Chest > Body
    REQUIRE(db.hitboxTypes[indices[0]] == HitboxType::Head);
    REQUIRE(db.hitboxTypes[indices[1]] == HitboxType::Chest);
    REQUIRE(db.hitboxTypes[indices[2]] == HitboxType::Body);
}

TEST_CASE("TargetSelector selectBest (full pipeline)", "[tracking][target_selector]") {
    TargetDatabase db;

    Vec2 crosshair{500.0f, 400.0f};
    float fov = 100.0f;

    // Add 4 targets
    db.addTarget(TargetID{1}, {510.0f, 410.0f}, {}, {}, 0.9f, HitboxType::Body, 0);    // Inside FOV, close, body
    db.addTarget(TargetID{2}, {515.0f, 415.0f}, {}, {}, 0.92f, HitboxType::Head, 0);   // Inside FOV, close, HEAD (best)
    db.addTarget(TargetID{3}, {700.0f, 400.0f}, {}, {}, 0.95f, HitboxType::Head, 0);   // Outside FOV (ignored)
    db.addTarget(TargetID{4}, {520.0f, 420.0f}, {}, {}, 0.88f, HitboxType::Chest, 0);  // Inside FOV, farther, chest

    std::optional<size_t> bestIndex = TargetSelector::selectBest(db, crosshair, fov);

    REQUIRE(bestIndex.has_value());
    REQUIRE(bestIndex.value() == 1);  // Target index 1 (id2, head hitbox, close)
}

TEST_CASE("TargetSelector no targets in FOV", "[tracking][target_selector]") {
    TargetDatabase db;

    Vec2 crosshair{500.0f, 400.0f};
    float fov = 50.0f;  // Small FOV

    // All targets outside FOV
    db.addTarget(TargetID{1}, {700.0f, 700.0f}, {}, {}, 0.9f, HitboxType::Head, 0);
    db.addTarget(TargetID{2}, {100.0f, 100.0f}, {}, {}, 0.85f, HitboxType::Chest, 0);

    std::optional<size_t> bestIndex = TargetSelector::selectBest(db, crosshair, fov);

    REQUIRE_FALSE(bestIndex.has_value());
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_target_selector -C Debug --output-on-failure`
Expected: FAIL with "TargetSelector.h: No such file or directory"

**Step 3: Implement TargetSelector.h**

Create `src/tracking/TargetSelector.h`:

```cpp
#pragma once

#include <vector>
#include <optional>
#include "core/entities/TargetDatabase.h"
#include "core/entities/MathTypes.h"

namespace macroman {

/**
 * @brief Target selection algorithm for aiming
 *
 * STRATEGY (in order):
 * 1. Filter by FOV (circular region around crosshair)
 * 2. Sort by distance to crosshair (ascending)
 * 3. Prioritize by hitbox type (Head > Chest > Body)
 * 4. Return first (closest, highest priority)
 */
class TargetSelector {
public:
    [[nodiscard]] static std::vector<size_t> filterByFOV(
        const TargetDatabase& targets,
        Vec2 crosshair,
        float fov
    ) noexcept;

    static void sortByDistance(
        const TargetDatabase& targets,
        std::vector<size_t>& indices,
        Vec2 crosshair
    ) noexcept;

    static void prioritizeByHitbox(
        const TargetDatabase& targets,
        std::vector<size_t>& indices
    ) noexcept;

    [[nodiscard]] static std::optional<size_t> selectBest(
        const TargetDatabase& targets,
        Vec2 crosshair,
        float fov
    ) noexcept;
};

} // namespace macroman
```

**Step 4: Implement TargetSelector.cpp**

Create `src/tracking/TargetSelector.cpp`:

```cpp
#include "TargetSelector.h"
#include <algorithm>

namespace macroman {

std::vector<size_t> TargetSelector::filterByFOV(const TargetDatabase& targets, Vec2 crosshair, float fov) noexcept {
    std::vector<size_t> result;
    result.reserve(targets.count);
    float fovSq = fov * fov;
    for (size_t i = 0; i < targets.count; ++i) {
        if (Vec2::distanceSquared(targets.positions[i], crosshair) <= fovSq) result.push_back(i);
    }
    return result;
}

void TargetSelector::sortByDistance(const TargetDatabase& targets, std::vector<size_t>& indices, Vec2 crosshair) noexcept {
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return Vec2::distanceSquared(targets.positions[a], crosshair) < Vec2::distanceSquared(targets.positions[b], crosshair);
    });
}

void TargetSelector::prioritizeByHitbox(const TargetDatabase& targets, std::vector<size_t>& indices) noexcept {
    std::stable_sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return static_cast<int>(targets.hitboxTypes[a]) > static_cast<int>(targets.hitboxTypes[b]);
    });
}

std::optional<size_t> TargetSelector::selectBest(const TargetDatabase& targets, Vec2 crosshair, float fov) noexcept {
    auto candidates = filterByFOV(targets, crosshair, fov);
    if (candidates.empty()) return std::nullopt;
    sortByDistance(targets, candidates, crosshair);
    prioritizeByHitbox(targets, candidates);
    return candidates[0];
}

} // namespace macroman
```

**Step 5: Update tracking CMakeLists.txt**

Modify `src/tracking/CMakeLists.txt`:

```cmake
add_library(tracking STATIC
    DataAssociation.cpp
    KalmanPredictor.cpp
    TargetSelector.cpp
)
```

**Step 6: Run test to verify it passes**

Run: `ctest --test-dir build -R test_target_selector -C Debug --output-on-failure`
Expected: PASS

**Step 7: Commit**

```bash
git add src/tracking/TargetSelector.h src/tracking/TargetSelector.cpp tests/unit/test_target_selector.cpp src/tracking/CMakeLists.txt
git commit -m "feat(tracking): implement target selection algorithm

- FOV filtering
- Distance sorting
- Hitbox prioritization (Head > Chest > Body)
- Unit tests for all selection stages"
```

---

## Task P3-07: AimCommand Structure

**Files:**
- Create: `src/core/entities/AimCommand.h`
- Create: `tests/unit/test_aim_command.cpp`

**Step 1: Write failing test for AimCommand**

Create `tests/unit/test_aim_command.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/AimCommand.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("AimCommand default construction", "[entities][aim_command]") {
    AimCommand cmd;

    REQUIRE_FALSE(cmd.hasTarget);
    REQUIRE_THAT(cmd.targetPosition.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cmd.targetPosition.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(cmd.confidence, WithinAbs(0.0f, 0.001f));
    REQUIRE(cmd.targetId.value == 0);
    REQUIRE(cmd.hitbox == HitboxType::Unknown);
}

TEST_CASE("AimCommand with target", "[entities][aim_command]") {
    AimCommand cmd;
    cmd.hasTarget = true;
    cmd.targetPosition = {320.0f, 240.0f};
    cmd.targetVelocity = {50.0f, -20.0f};
    cmd.confidence = 0.92f;
    cmd.targetId = TargetID{42};
    cmd.hitbox = HitboxType::Head;
    cmd.predictionTimeNs = 15000000;  // 15ms in nanoseconds

    REQUIRE(cmd.hasTarget);
    REQUIRE_THAT(cmd.targetPosition.x, WithinAbs(320.0f, 0.001f));
    REQUIRE_THAT(cmd.targetVelocity.x, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(cmd.confidence, WithinAbs(0.92f, 0.001f));
    REQUIRE(cmd.targetId == TargetID{42});
    REQUIRE(cmd.hitbox == HitboxType::Head);
}

TEST_CASE("AimCommand isValid", "[entities][aim_command]") {
    SECTION("Valid command") {
        AimCommand cmd;
        cmd.hasTarget = true;
        cmd.confidence = 0.8f;

        REQUIRE(cmd.isValid());
    }

    SECTION("Invalid: no target") {
        AimCommand cmd;
        cmd.hasTarget = false;
        cmd.confidence = 0.9f;

        REQUIRE_FALSE(cmd.isValid());
    }

    SECTION("Invalid: low confidence") {
        AimCommand cmd;
        cmd.hasTarget = true;
        cmd.confidence = 0.1f;

        REQUIRE_FALSE(cmd.isValid());
    }
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_aim_command -C Debug --output-on-failure`
Expected: FAIL with "AimCommand.h: No such file or directory"

**Step 3: Implement AimCommand.h**

Create `src/core/entities/AimCommand.h`:

```cpp
#pragma once

#include <cstdint>
#include "MathTypes.h"
#include "Detection.h"

namespace macroman {

/**
 * @brief Command from Tracking Thread to Input Thread
 *
 * SYNCHRONIZATION: Atomic exchange (single writer, single reader)
 * sizeof(AimCommand) should be small (<64 bytes for lock-free guarantee)
 */
struct AimCommand {
    bool hasTarget{false};                  // True if valid target selected
    Vec2 targetPosition{0.0f, 0.0f};        // Predicted screen position (pixels)
    Vec2 targetVelocity{0.0f, 0.0f};        // Target velocity (pixels/second)
    float confidence{0.0f};                 // Detection confidence [0.0, 1.0]
    TargetID targetId{0};                   // Target identifier (for debugging)
    HitboxType hitbox{HitboxType::Unknown}; // Hitbox type (for aim offset)
    int64_t predictionTimeNs{0};            // Time this prediction is valid for (nanoseconds)

    /**
     * @brief Check if command is actionable
     */
    [[nodiscard]] bool isValid() const noexcept {
        return hasTarget && confidence > 0.3f;
    }
};

// Verify AimCommand can be used with std::atomic
static_assert(std::is_trivially_copyable_v<AimCommand>,
              "AimCommand must be trivially copyable for std::atomic");

} // namespace macroman
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R test_aim_command -C Debug --output-on-failure`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/entities/AimCommand.h tests/unit/test_aim_command.cpp
git commit -m "feat(core): add AimCommand structure for inter-thread communication

- Atomic-ready (trivially copyable)
- Contains target position, velocity, confidence, ID, and hitbox info"
```

---

## Task P3-08: TargetTracker Implementation (with Track Maintenance)

**Files:**
- Create: `src/tracking/TargetTracker.h`
- Create: `src/tracking/TargetTracker.cpp`
- Create: `tests/unit/test_target_tracker.cpp`

**Step 1: Write failing test for TargetTracker**

Create `tests/unit/test_target_tracker.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "tracking/TargetTracker.h"

using namespace macroman;
using Catch::Matchers::WithinAbs;

TEST_CASE("TargetTracker track maintenance", "[tracking][target_tracker]") {
    TargetTracker tracker;

    // Frame 1: Add target
    DetectionBatch batch1;
    batch1.observations.push_back({{100.0f, 100.0f, 20.0f, 20.0f}, 0.9f, 0, HitboxType::Head});
    batch1.captureTimeNs = 0;
    tracker.update(batch1);
    REQUIRE(tracker.getTargetCount() == 1);

    // Frame 2: Observation lost, but within grace period
    DetectionBatch batch2;
    batch2.captureTimeNs = 50 * 1000 * 1000; // 50ms later
    tracker.update(batch2);
    REQUIRE(tracker.getTargetCount() == 1); // Should still exist (Coast state)

    // Frame 3: Observation lost, beyond grace period (100ms)
    DetectionBatch batch3;
    batch3.captureTimeNs = 150 * 1000 * 1000; // 150ms total since last seen
    tracker.update(batch3);
    REQUIRE(tracker.getTargetCount() == 0); // Should be removed
}

TEST_CASE("TargetTracker prediction logic", "[tracking][target_tracker]") {
    TargetTracker tracker;
    DetectionBatch batch;
    batch.observations.push_back({{500.0f, 400.0f, 20.0f, 20.0f}, 0.9f, 0, HitboxType::Head});
    batch.captureTimeNs = 0;
    tracker.update(batch);

    // Select best target with 16ms prediction ahead
    auto cmd = tracker.selectBestTarget({500.0f, 400.0f}, 100.0f, 0.016f);
    REQUIRE(cmd.has_value());
    REQUIRE(cmd->hasTarget);
}
```

**Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R test_target_tracker -C Debug --output-on-failure`
Expected: FAIL

**Step 3: Implement TargetTracker.h**

Create `src/tracking/TargetTracker.h`:

```cpp
#pragma once

#include <optional>
#include <vector>
#include "core/entities/TargetDatabase.h"
#include "core/entities/DetectionBatch.h"
#include "core/entities/AimCommand.h"
#include "DataAssociation.h"
#include "KalmanPredictor.h"
#include "TargetSelector.h"

namespace macroman {

class TargetTracker {
public:
    TargetTracker();

    /**
     * @brief Update tracks with new detections and perform maintenance
     */
    void update(const DetectionBatch& batch) noexcept;

    /**
     * @brief Select target and extrapolate position
     */
    [[nodiscard]] std::optional<AimCommand> selectBestTarget(
        Vec2 crosshair,
        float fov,
        float predictionTimeS
    ) noexcept;

    [[nodiscard]] size_t getTargetCount() const noexcept { return db_.count; }

private:
    TargetDatabase db_;
    KalmanPredictor kalman_;
    uint64_t nextId_{1};

    // Track Maintenance Parameters
    static constexpr int64_t GRACE_PERIOD_NS = 100 * 1000 * 1000; // 100ms
};

} // namespace macroman
```

**Step 4: Implement TargetTracker.cpp**

Create `src/tracking/TargetTracker.cpp`:

```cpp
#include "TargetTracker.h"
#include <algorithm>

namespace macroman {

TargetTracker::TargetTracker() : kalman_(10.0f, 5.0f) {}

void TargetTracker::update(const DetectionBatch& batch) noexcept {
    // 1. Prediction update (Bulk SoA)
    // Note: dt is not used in bulk update here since we rely on Kalman per-track dt
    // but we can use it for simple extrapolation if needed.

    // 2. Data Association
    auto result = DataAssociation::matchDetections(db_, batch.observations);

    // 3. Update Matched Tracks
    for (const auto& match : result.matches) {
        size_t tIdx = match.targetIndex;
        const auto& det = batch.observations[match.detectionIndex];

        float dt = (batch.captureTimeNs - db_.lastSeenNs[tIdx]) / 1e9f;
        if (dt <= 0) dt = 0.016f; // Safe default for first update

        // Update Kalman Filter state stored in SoA
        kalman_.update(det.bbox.center(), dt, db_.kalmanStates[tIdx]);

        // Sync SoA fields
        db_.positions[tIdx] = {db_.kalmanStates[tIdx].x, db_.kalmanStates[tIdx].y};
        db_.velocities[tIdx] = {db_.kalmanStates[tIdx].vx, db_.kalmanStates[tIdx].vy};
        db_.bboxes[tIdx] = det.bbox;
        db_.confidences[tIdx] = det.confidence;
        db_.lastSeenNs[tIdx] = batch.captureTimeNs;
    }

    // 4. Add New Tracks
    for (size_t dIdx : result.unmatchedDetections) {
        const auto& det = batch.observations[dIdx];
        size_t tIdx = db_.addTarget(TargetID{nextId_++}, det.bbox.center(), {0,0}, det.bbox, det.confidence, det.hitbox, batch.captureTimeNs);
        
        if (tIdx < TargetDatabase::MAX_TARGETS) {
            // Initialize Kalman state
            db_.kalmanStates[tIdx].x = det.bbox.center().x;
            db_.kalmanStates[tIdx].y = det.bbox.center().y;
        }
    }

    // 5. Track Maintenance (Grace Period)
    for (int i = static_cast<int>(db_.count) - 1; i >= 0; --i) {
        size_t idx = static_cast<size_t>(i);
        
        // If unmatched this frame, check grace period
        bool matched = std::any_of(result.matches.begin(), result.matches.end(), 
            [idx](const auto& m) { return m.targetIndex == idx; });

        if (!matched) {
            int64_t timeSinceLastSeen = batch.captureTimeNs - db_.lastSeenNs[idx];
            if (timeSinceLastSeen > GRACE_PERIOD_NS) {
                db_.removeTarget(idx);
            } else {
                // Coasting: update position using last known velocity
                float dt = 0.016f; // Estimate
                db_.positions[idx].x += db_.velocities[idx].x * dt;
                db_.positions[idx].y += db_.velocities[idx].y * dt;
                // Decay confidence
                db_.confidences[idx] *= 0.9f;
            }
        }
    }
}

std::optional<AimCommand> TargetTracker::selectBestTarget(Vec2 crosshair, float fov, float predictionTimeS) noexcept {
    auto bestIdx = TargetSelector::selectBest(db_, crosshair, fov);
    if (!bestIdx) return std::nullopt;

    size_t idx = *bestIdx;
    AimCommand cmd;
    cmd.hasTarget = true;
    cmd.targetId = db_.ids[idx];
    cmd.hitbox = db_.hitboxTypes[idx];
    cmd.confidence = db_.confidences[idx];
    
    // Extrapolate position for Input Thread
    cmd.targetPosition = kalman_.predict(predictionTimeS, db_.kalmanStates[idx]);
    cmd.targetVelocity = db_.velocities[idx];
    cmd.predictionTimeNs = static_cast<int64_t>(predictionTimeS * 1e9f);

    return cmd;
}

} // namespace macroman
```

**Step 5: Run tests**

Run: `cmake --build build && ctest --test-dir build -R test_target_tracker`
Expected: ALL PASS

**Step 6: Commit**

```bash
git add src/tracking/TargetTracker.h src/tracking/TargetTracker.cpp tests/unit/test_target_tracker.cpp
git commit -m "feat(tracking): implement TargetTracker with track maintenance

- Grace period (100ms) for lost observations
- Coasting state using last known velocity
- Target extrapolation for aim commands
- Full integration of SoA TargetDatabase and stateless Kalman predictor"
```

---

## Execution Handoff

Plan updated with SoA fixes, stateless Kalman integration, and track maintenance grace periods.

Ready for execution.