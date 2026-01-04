# Pull Request Review: Phase 8 Optimization & Polish

**Reviewer:** Gemini Agent
**Date:** 2026-01-03
**Status:** 🔴 **REQUEST CHANGES**

## 1. Executive Summary

This PR implements critical performance optimizations (SIMD, Thread Affinity, Lock-free Metrics) but **fails to build** when profiling is enabled (`ENABLE_TRACY=ON`) and the provided benchmark tool **fails to validate** the primary optimization (P8-02 SIMD).

The code changes themselves are high-quality and logically sound, but the integration and verification layers require immediate attention before merging.

## 2. Critical Issues (Must Fix)

### 🔴 Build Failure (P8-01 Tracy Integration)
The build fails with `fatal error C1083: Cannot open include file: 'Tracy.hpp'` in `src/input/InputManager.cpp` and `src/core/profiling/Profiler.h`.

*   **Cause:** `macroman_core` and `macroman_input` libraries do not link against the `Tracy::TracyClient` target, preventing include paths from propagating.
*   **Impact:** Cannot build with profiling enabled.

### 🔴 Benchmark Validity (P8-02 Verification)
The `macroman-bench` tool is insufficient for validating this PR:
1.  **Missing SIMD Workload:** The benchmark only tests `Capture` -> `Detection`. It **does not** execute `TargetTracker` or `TargetDatabase::updatePredictions`, effectively bypassing the SIMD optimizations (P8-02) entirely.
2.  **Broken Delay Simulation:** The benchmark reports `0.00 ms` latency and >100k FPS despite a requested 6ms inference delay. The `FakeDetector` sleep logic appears ineffective or `inferenceDelayMs` is not being set correctly in the test runtime.

### 🔴 Unit Test Availability
Unit tests (`unit_tests`) were not built/runnable due to the aforementioned build failures in `macroman_input`.

## 3. Implementation Plan (For Developers)

Please follow these steps to resolve the issues and merge the PR.

### Step 1: Fix Build Dependencies
Update `CMakeLists.txt` in `src/core` and `src/input` to link Tracy correctly.

**File:** `src/core/CMakeLists.txt`
```cmake
# Add to end of file
if(ENABLE_TRACY)
    target_link_libraries(macroman_core PUBLIC Tracy::TracyClient)
endif()
```

**File:** `src/input/CMakeLists.txt`
```cmake
# Add to end of file
if(ENABLE_TRACY)
    target_link_libraries(macroman_input PUBLIC Tracy::TracyClient)
endif()
```

### Step 2: Fix Benchmark Tool (`tests/benchmark/macroman-bench.cpp`)
Update the benchmark to include the Tracking stage.

1.  Include `core/tracking/TargetTracker.h`.
2.  Instantiate `TargetTracker` (mocking config if necessary).
3.  In the loop, after `detector.detect(frame)`, pass results to `tracker.update(...)`.
4.  Call `tracker.predict(dt)` to trigger `TargetDatabase::updatePredictions`.

This ensures the AVX2 SIMD path is actually executed and measured.

### Step 3: Debug FakeDetector Sleep
Investigate why `FakeDetector` is not sleeping.
*   Check if `args.inferenceDelayMs` is parsed correctly.
*   Verify `FakeDetector::setInferenceDelay` is called.
*   **Verify:** Benchmark should report ~160 FPS (for 6ms delay) instead of >100k FPS.

### Step 4: Optimization (Optional)
In `src/core/entities/TargetDatabase.h`, the arrays are aligned to 32 bytes (`alignas(32)`).
*   **Current:** `_mm256_loadu_ps` (Unaligned load)
*   **Suggestion:** Use `_mm256_load_ps` (Aligned load) for potentially slightly better performance, as `MAX_TARGETS=64` guarantees 32-byte alignment for every 4th element stride.

## 4. Verification Checklist
- [ ] Build succeeds with `-DENABLE_TRACY=ON`.
- [ ] `unit_tests` pass (`ctest`).
- [ ] `macroman-bench` runs and reports realistic latency (>0ms).
- [ ] `macroman-bench` explicitly exercises `TargetTracker`.

---
**Note to Maintainer:** Do not merge until CI passes with `ENABLE_TRACY=ON`.
