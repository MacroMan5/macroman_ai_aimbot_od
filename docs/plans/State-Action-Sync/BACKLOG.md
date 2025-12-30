# Project Backlog: MacroMan AI Aimbot v2

This backlog tracks granular tasks across all development phases.

---

## Phase 1: Foundation 🟡
- [x] Project directory structure initialization
- [ ] Refactor root CMake to modular interface-driven structure
- [x] Define/Audit Core Interfaces (`IScreenCapture`, `IDetector`, `IMouseDriver`)
- [ ] Implement `LatestFrameQueue` (Lock-free head-drop)
- [ ] Implement `ThreadManager` (Windows-specific priorities)
- [ ] Integrate `spdlog` Logging system wrapper
- [ ] Setup Catch2 Unit Testing framework integration
- [ ] Phase 1 Integration Demo (`main.cpp`)

## Phase 2: Capture & Detection ⚪
- [ ] Audit/Refactor `WinrtCapture` implementation
- [ ] Audit/Refactor `DuplicationCapture` fallback
- [ ] Implement Texture Pool (Triple buffer with atomic ref-counting)
- [ ] Audit/Refactor `DMLDetector` (DirectML/ONNX Runtime)
- [ ] Audit/Refactor `TensorRTDetector` (NVIDIA Backend)
- [ ] Audit/Refactor NMS (Non-Maximum Suppression) Post-processing

## Phase 3: Tracking & Prediction ⚪
- [ ] Implement/Audit `TargetDatabase` (SoA structure)
- [ ] Audit `KalmanPredictor` implementation
- [ ] Implement Target selection logic (FOV + Distance + Hitbox)
- [ ] Implement unified timestamping system across pipeline

## Phase 4: Input & Aiming ⚪
- [ ] Audit `Win32Driver` (SendInput)
- [ ] Audit `ArduinoDriver` (Serial HID)
- [ ] Audit `BezierCurve` and `TrajectoryPlanner` implementation
- [ ] Audit `OneEuroFilter` smoothing
- [ ] Implement 1000Hz Input Loop orchestration

## Phase 5: Configuration & Auto-Detection ⚪
- [ ] Implement Game Profile JSON system
- [ ] Implement `GameDetector` (Auto-detection with hysteresis)
- [ ] Implement `SharedConfig` (Memory-mapped IPC for live tuning)
- [ ] Implement ModelManager (Thread-safe model switching)

## Phase 6: Safety & Humanization ⚪
- [ ] Reaction Delay Manager (Simulate human reaction time)
- [ ] Micro-Tremor/Jitter Simulator
- [ ] Bezier Overshoot & Correction logic
- [ ] SetWindowDisplayAffinity (Screenshot protection)

## Phase 7: Testing & Benchmarking ⚪
- [ ] Complete unit test coverage for algorithms
- [ ] CLI benchmark tool (`sunone-bench.exe`)
- [ ] Integration tests with golden datasets (500 frames)
- [ ] GitHub Actions CI/CD Pipeline

## Phase 8: Optimization & Polish ⚪
- [ ] Tracy Profiler integration
- [ ] SIMD acceleration for `TargetDatabase`
- [ ] High-resolution Windows timer implementation
- [ ] GPU Resource Monitoring (NVML)
- [ ] Cache Line Optimization (False Sharing)

## Phase 9: Documentation ⚪
- [ ] Doxygen API Documentation
- [ ] Technical Developer Guide
- [ ] User Setup & Calibration Guide
- [ ] Safety, Ethics, & Humanization Manual

## Phase 10: UI & Observability ⚪
- [ ] ImGui Debug Overlay (Metrics + BBoxes)
- [ ] Standalone Config UI Application
- [ ] FrameProfiler (Latency Graphs)
- [ ] Lock-free Telemetry System

---

## ✅ Completed Tasks
- [2025-12-30] Standardized all 10 implementation plans.
- [2025-12-30] Defined State-Action-Sync workflow in `CLAUDE.md`.
- [2025-12-30] Initial architectural audit and backlog creation.
