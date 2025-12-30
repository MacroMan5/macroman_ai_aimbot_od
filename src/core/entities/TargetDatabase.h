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
