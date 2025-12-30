# Phase 8: Optimization & Polish - Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Achieve production-ready performance (<10ms latency, 144+ FPS, <512MB VRAM, <1GB RAM) through profiling-driven optimization, SIMD acceleration, and comprehensive stress testing.

**Architecture:** Profile hot paths with Tracy/VTune, apply SIMD to TargetDatabase updates, tune thread affinity for cache locality, optimize GPU resource usage with CUDA streams and NVML monitoring, implement robust error handling with graceful degradation.

**Tech Stack:** Tracy Profiler 0.10+, Intel VTune 2024, AVX2 SIMD intrinsics, NVML (NVIDIA Management Library), CUDA Runtime API, Windows Performance Counters.

---

## Task P8-01: Tracy Profiler Integration
**Goal:** Instrument the 4-thread pipeline with zone markers and value tracking.
- **Action:** Add Tracy to CMake, create `Profiler.h` wrapper, and add `ZONE_SCOPED` markers to Capture, Detection, Tracking, and Input loops.
- **Verification:** Connect Tracy GUI and verify that end-to-end latency and queue sizes are visible.

## Task P8-02: SIMD Acceleration (TargetDatabase)
**Goal:** Accelerate target prediction using AVX2 (4x speedup).
- **Action:** Implement `updatePredictionsSIMD` using `_mm256_fmadd_ps` for the Structure-of-Arrays (SoA) layout.
- **Verification:** Run `tests/unit/test_target_database_simd.cpp` and verify >4x speedup over scalar implementation.

## Task P8-03: Thread Affinity & Priority Tuning
**Goal:** Reduce context-switch jitter.
- **Action:** Use `SetThreadAffinityMask` to pin threads to cores 1-4 (avoiding Core 0). Set `THREAD_PRIORITY_TIME_CRITICAL` for Capture and `THREAD_PRIORITY_HIGHEST` for Input.
- **Verification:** Use Tracy "Context Switches" view to verify threads stay on assigned cores.

## Task P8-04: High-Resolution Timing (Windows)
**Goal:** Ensure 1ms sleep accuracy for the 1000Hz Input Loop.
- **Action:** Implement `timeBeginPeriod(1)` at startup and `timeEndPeriod(1)` at shutdown.
- **Verification:** Verify `InputLoop` frequency in Tracy is exactly 1000Hz (+/- 5%).

## Task P8-05: Cache Line Optimization (False Sharing)
**Goal:** Prevent cache-line contention in `Metrics` and `SharedConfig`.
- **Action:** Use `alignas(64)` and padding for atomic variables in IPC/Metrics structures to ensure they reside on separate cache lines.
- **Verification:** Run performance benchmark; verify no regression when metrics collection frequency is increased.

## Task P8-06: GPU Resource Monitoring (NVML)
**Goal:** Enforce the <512MB VRAM and <20% Compute budget.
- **Action:** Implement `GPUMonitor` using NVIDIA Management Library. Add background poll (500ms).
- **Verification:** Verify VRAM usage is correctly reported in the debug overlay.

## Task P8-07: Memory Pool & Hot-Path Audit
**Goal:** Eliminate all heap allocations in the frame-processing loop.
- **Action:** Audit `TexturePool` and `DetectionBatch` for `new`/`delete`. Enforce `FixedCapacityVector` for all detection results.
- **Verification:** Run with a heap profiler (like Valgrind or Dr. Memory/vld) and verify 0 allocations/sec during active detection.

## Task P8-08: Circuit Breaker & Graceful Degradation
**Goal:** Prevent cascading failures if a component (like Detection) hangs.
- **Action:** Implement `ErrorHandler` for each thread. If failures > threshold, "Open" the circuit to skip processing and prevent system lag.
- **Verification:** Manually inject a 500ms delay in Detection; verify the pipeline skips frames rather than queuing them up.

## Task P8-09: 1-Hour Stability Stress Test
**Goal:** Validate long-term resource safety.
- **Action:** Run `scripts/run_stress_test.ps1`. Monitor for memory leaks or VRAM growth.
- **Verification:** P95 latency must remain <15ms for the entire hour. No TexturePool starvation events.

## Task P8-10: Multi-Game Profile Validation
**Goal:** Ensure settings don't "bleed" between game profiles.
- **Action:** Automate switching between CS2, Valorant, and Apex profiles while under load.
- **Verification:** Verify model swap completes in <3 seconds and config atomics are updated correctly.

---

## Deliverables
- [ ] Tracy-instrumented executable.
- [ ] SIMD-accelerated `TargetDatabase`.
- [ ] High-resolution Windows timer implementation.
- [ ] `GPUMonitor` service (NVML).
- [ ] Stress test report (`docs/benchmarks/stress_test_v1.md`).