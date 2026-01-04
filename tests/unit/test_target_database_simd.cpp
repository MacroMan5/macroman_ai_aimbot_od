/**
 * @file test_target_database_simd.cpp
 * @brief Unit tests for TargetDatabase SIMD acceleration (Phase 8 - P8-02)
 *
 * Tests:
 * - Correctness: SIMD matches scalar output (epsilon tolerance)
 * - Performance: SIMD is faster than scalar (4x speedup target)
 * - Edge cases: Works with count % 4 != 0 (scalar fallback)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/entities/TargetDatabase.h"
#include <chrono>

using namespace macroman;
using Catch::Matchers::WithinRel;

/**
 * @brief Scalar reference implementation (for correctness validation)
 */
void updatePredictionsScalar(TargetDatabase& db, float dt) {
    for (size_t i = 0; i < db.count; ++i) {
        db.positions[i].x += db.velocities[i].x * dt;
        db.positions[i].y += db.velocities[i].y * dt;
    }
}

TEST_CASE("TargetDatabase SIMD - Correctness", "[simd][target-database]") {
    SECTION("SIMD matches scalar for aligned count (multiple of 4)") {
        // Create two identical databases
        TargetDatabase simd_db;
        TargetDatabase scalar_db;

        // Add 16 targets (perfect alignment for AVX2)
        for (size_t i = 0; i < 16; ++i) {
            Vec2 pos = {100.0f + static_cast<float>(i) * 10.0f, 200.0f + static_cast<float>(i) * 5.0f};
            Vec2 vel = {1.5f, -0.5f};
            simd_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
            scalar_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
        }

        float dt = 0.016f;  // 16ms (60 FPS)

        // Run both implementations
        simd_db.updatePredictions(dt);
        updatePredictionsScalar(scalar_db, dt);

        // Verify all positions match
        for (size_t i = 0; i < 16; ++i) {
            REQUIRE_THAT(simd_db.positions[i].x, WithinRel(scalar_db.positions[i].x, 0.0001f));
            REQUIRE_THAT(simd_db.positions[i].y, WithinRel(scalar_db.positions[i].y, 0.0001f));
        }
    }

    SECTION("SIMD matches scalar for unaligned count (not multiple of 4)") {
        // Test edge case: 13 targets (SIMD processes 12, scalar fallback for 1)
        TargetDatabase simd_db;
        TargetDatabase scalar_db;

        for (size_t i = 0; i < 13; ++i) {
            Vec2 pos = {50.0f + static_cast<float>(i) * 20.0f, 150.0f};
            Vec2 vel = {2.0f, 1.0f};
            simd_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Head, 0);
            scalar_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Head, 0);
        }

        float dt = 0.033f;  // 33ms (30 FPS)

        simd_db.updatePredictions(dt);
        updatePredictionsScalar(scalar_db, dt);

        for (size_t i = 0; i < 13; ++i) {
            REQUIRE_THAT(simd_db.positions[i].x, WithinRel(scalar_db.positions[i].x, 0.0001f));
            REQUIRE_THAT(simd_db.positions[i].y, WithinRel(scalar_db.positions[i].y, 0.0001f));
        }
    }

    SECTION("SIMD matches scalar for full database (64 targets)") {
        TargetDatabase simd_db;
        TargetDatabase scalar_db;

        for (size_t i = 0; i < 64; ++i) {
            Vec2 pos = {static_cast<float>(i % 10) * 100.0f, static_cast<float>(i / 10) * 50.0f};
            Vec2 vel = {static_cast<float>(i % 5) - 2.0f, static_cast<float>(i % 3) - 1.0f};
            simd_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
            scalar_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
        }

        float dt = 0.016f;

        simd_db.updatePredictions(dt);
        updatePredictionsScalar(scalar_db, dt);

        for (size_t i = 0; i < 64; ++i) {
            REQUIRE_THAT(simd_db.positions[i].x, WithinRel(scalar_db.positions[i].x, 0.0001f));
            REQUIRE_THAT(simd_db.positions[i].y, WithinRel(scalar_db.positions[i].y, 0.0001f));
        }
    }
}

TEST_CASE("TargetDatabase SIMD - Edge Cases", "[simd][target-database]") {
    SECTION("Empty database (count = 0)") {
        TargetDatabase db;
        db.updatePredictions(0.016f);  // noexcept function, no exceptions expected
        REQUIRE(db.count == 0);
    }

    SECTION("Single target (scalar fallback)") {
        TargetDatabase db;
        db.addTarget(TargetID{1}, Vec2{100.0f, 100.0f}, Vec2{10.0f, -5.0f}, BBox{}, 0.9f, HitboxType::Head, 0);

        float dt = 0.016f;
        db.updatePredictions(dt);

        REQUIRE_THAT(db.positions[0].x, WithinRel(100.0f + 10.0f * dt, 0.0001f));
        REQUIRE_THAT(db.positions[0].y, WithinRel(100.0f - 5.0f * dt, 0.0001f));
    }

    SECTION("Zero velocity (no movement)") {
        TargetDatabase db;
        for (size_t i = 0; i < 8; ++i) {
            db.addTarget(TargetID{i}, Vec2{100.0f, 200.0f}, Vec2{0.0f, 0.0f}, BBox{}, 0.9f, HitboxType::Body, 0);
        }

        Vec2 originalPos = db.positions[0];
        db.updatePredictions(0.016f);

        for (size_t i = 0; i < 8; ++i) {
            REQUIRE_THAT(db.positions[i].x, WithinRel(originalPos.x, 0.0001f));
            REQUIRE_THAT(db.positions[i].y, WithinRel(originalPos.y, 0.0001f));
        }
    }

    SECTION("Negative velocities") {
        TargetDatabase db;
        db.addTarget(TargetID{1}, Vec2{500.0f, 400.0f}, Vec2{-20.0f, -10.0f}, BBox{}, 0.9f, HitboxType::Body, 0);
        db.addTarget(TargetID{2}, Vec2{600.0f, 300.0f}, Vec2{-15.0f, -5.0f}, BBox{}, 0.9f, HitboxType::Body, 0);

        float dt = 0.1f;
        db.updatePredictions(dt);

        REQUIRE_THAT(db.positions[0].x, WithinRel(500.0f - 20.0f * dt, 0.0001f));
        REQUIRE_THAT(db.positions[0].y, WithinRel(400.0f - 10.0f * dt, 0.0001f));
    }
}

TEST_CASE("TargetDatabase SIMD - Performance Benchmark", "[simd][target-database][benchmark]") {
    SECTION("Performance: SIMD vs Scalar (64 targets, 1000 iterations)") {
        TargetDatabase simd_db;
        TargetDatabase scalar_db;

        // Fill with 64 targets
        for (size_t i = 0; i < 64; ++i) {
            Vec2 pos = {static_cast<float>(i * 10), static_cast<float>(i * 5)};
            Vec2 vel = {2.0f, -1.0f};
            simd_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
            scalar_db.addTarget(TargetID{i}, pos, vel, BBox{}, 0.9f, HitboxType::Body, 0);
        }

        const int iterations = 1000;
        float dt = 0.016f;

        // Benchmark SIMD
        auto simd_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            simd_db.updatePredictions(dt);
        }
        auto simd_end = std::chrono::high_resolution_clock::now();
        auto simd_duration = std::chrono::duration_cast<std::chrono::microseconds>(simd_end - simd_start).count();

        // Benchmark Scalar
        auto scalar_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            updatePredictionsScalar(scalar_db, dt);
        }
        auto scalar_end = std::chrono::high_resolution_clock::now();
        auto scalar_duration = std::chrono::duration_cast<std::chrono::microseconds>(scalar_end - scalar_start).count();

        // Calculate speedup
        float speedup = static_cast<float>(scalar_duration) / static_cast<float>(simd_duration);

        // Log results
        INFO("SIMD: " << simd_duration << " μs");
        INFO("Scalar: " << scalar_duration << " μs");
        INFO("Speedup: " << speedup << "x");

        // Verify SIMD is faster (target: >2x speedup, ideal: 4x)
        REQUIRE(speedup > 2.0f);

        // Verify correctness wasn't sacrificed for performance
        for (size_t i = 0; i < 64; ++i) {
            REQUIRE_THAT(simd_db.positions[i].x, WithinRel(scalar_db.positions[i].x, 0.001f));
            REQUIRE_THAT(simd_db.positions[i].y, WithinRel(scalar_db.positions[i].y, 0.001f));
        }
    }

    SECTION("Performance: No regression with small counts") {
        TargetDatabase db;

        // Test with 3 targets (all scalar fallback)
        for (size_t i = 0; i < 3; ++i) {
            db.addTarget(TargetID{i}, Vec2{100.0f, 100.0f}, Vec2{1.0f, 1.0f}, BBox{}, 0.9f, HitboxType::Body, 0);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            db.updatePredictions(0.016f);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        INFO("10,000 iterations with 3 targets: " << duration << " μs");

        // Verify no catastrophic slowdown (should be <1ms for 10k iterations)
        REQUIRE(duration < 1000);
    }
}
