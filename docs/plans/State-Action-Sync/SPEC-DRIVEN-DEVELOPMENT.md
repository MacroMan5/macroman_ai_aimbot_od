# Spec-Driven Development: Phase-by-Phase Requirements

This document defines the strict **specifications**, **expected behaviors**, and **error handling** for every development phase. Use this as the ultimate checklist for "Definition of Done."

---

## Phase 1: Foundation
*   **Spec**: C++20 modular build system with thread-safe logging and a lock-free queue.
*   **Behavior**: `LatestFrameQueue` must return the most recent item and delete all skipped items immediately.
*   **Error Handling**:
    *   Fail build if C++20 is not supported.
    *   Log `CRITICAL` if `LatestFrameQueue` is not lock-free on the target CPU.
    *   `ThreadManager` must log a `WARNING` if a thread fails to stop within 5 seconds.

## Phase 2: Capture & Detection
*   **Spec**: WinRT/Desktop Duplication capture + ONNX Runtime (DirectML/TensorRT).
*   **Behavior**: Zero-copy path from capture to AI inference. `TexturePool` must handle all GPU resource allocations.
*   **Error Handling**:
    *   **Trap 1 (Leak on Drop)**: `Frame` MUST use an RAII deleter to return textures to the pool.
    *   If `TexturePool` is empty, the capture thread must drop the frame and increment `texturePoolStarved`.
    *   Re-initialize capture automatically on `DXGI_ERROR_ACCESS_LOST`.

## Phase 3: Tracking & Prediction
*   **Spec**: Multi-target tracking using Kalman Filters and IoU data association.
*   **Behavior**: System must maintain a "Coast" state for targets lost for <100ms.
*   **Error Handling**:
    *   If TargetDatabase reaches 64 targets, reject new detections until old ones expire.
    *   Invalidate tracks if the Kalman Filter covariance matrix becomes non-finite (NaN).

## Phase 4: Input & Aiming
*   **Spec**: 1000Hz loop with Bezier smoothing and 1 Euro Filter.
*   **Behavior**: Mouse deltas must be calculated relative to the current crosshair, not absolute screen coordinates.
*   **Error Handling**:
    *   **Trap 2 (Extrapolation)**: Clamp prediction `dt` at 50ms. Decay confidence by 50% if data is >50ms old.
    *   **Trap 4 (Deadman Switch)**: Stop all movement if no `AimCommand` is received for >200ms.

## Phase 5: Configuration & Auto-Detection
*   **Spec**: JSON profile loading + Memory-mapped IPC.
*   **Behavior**: Hysteresis timer (3s) must pass before switching game models to avoid "alt-tab flickering."
*   **Error Handling**:
    *   **Trap 3 (IPC Atomics)**: `static_assert` that all `SharedConfig` atomics are `is_always_lock_free`.
    *   Validate JSON ranges (e.g., FOV cannot be negative) and load "Safe Defaults" on failure.

## Phase 6: Safety & Humanization
*   **Spec**: Reaction delays, micro-tremors, and screenshot protection.
*   **Behavior**: Reaction time must follow a Normal Distribution (μ=160ms, σ=25ms).
*   **Error Handling**:
    *   Log `CRITICAL` if `SetWindowDisplayAffinity` fails (as overlay privacy is compromised).

## Phase 7: Testing & Benchmarking
*   **Spec**: Catch2 unit tests + CLI benchmark tool.
*   **Behavior**: Benchmarks must run on headless servers (WARP renderer fallback).
*   **Error Handling**:
    *   Fail the CI/CD pipeline if P99 latency exceeds 15ms or average FPS drops below 120.

## Phase 8: Optimization & Polish
*   **Spec**: SIMD acceleration + Tracy profiling.
*   **Behavior**: ZERO heap allocations in the "Hot Path" (Capture → Move) after initial warmup.
*   **Error Handling**:
    *   Circuit Breaker: If Detection latency consistently >30ms, disable aiming automatically to prevent system lag.

## Phase 9: Documentation
*   **Spec**: Doxygen API docs + User/Developer Guides.
*   **Behavior**: All "Hot Path" functions must be explicitly labeled in documentation to warn against heap allocation.

## Phase 10: UI & Observability
*   **Spec**: ImGui transparent overlay and external control app.
*   **Behavior**: Overlay must remain responsive (60 FPS) even if the Core Engine is struggling.
*   **Error Handling**:
    *   Display "Safety Alerts" in the UI if any Critical Traps are triggered (e.g., Deadman Switch active).
