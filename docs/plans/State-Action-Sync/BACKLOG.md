# Project Backlog: MacroMan AI Aimbot v2

This backlog tracks granular tasks across all development phases.

---

## Phase 1: Foundation ✅ **COMPLETE**
- [x] Project directory structure initialization
- [x] Refactor root CMake to modular interface-driven structure
- [x] Define/Audit Core Interfaces (`IScreenCapture`, `IDetector`, `IMouseDriver`)
- [x] Implement `LatestFrameQueue` (Lock-free head-drop)
- [x] Implement `ThreadManager` (Windows-specific priorities)
- [x] Integrate `spdlog` Logging system wrapper
- [x] Setup Catch2 Unit Testing framework integration
- [x] Global namespace rename: sunone → macroman
- [x] Unit tests verified (12 assertions passing)
- [x] Build system functional

## Phase 2: Capture & Detection ✅ **COMPLETE**
- [x] Implement Texture Pool (Triple buffer with RAII TextureHandle)
- [x] Implement `DuplicationCapture` (Desktop Duplication API)
- [x] Implement Input Preprocessing (Compute Shader for BGRA→RGB tensor)
- [x] Implement NMS Post-processing (IoU-based overlap removal)
- [x] Unit tests for TexturePool and PostProcessor
- [x] Build system integration (CMake for capture and detection modules)
- [ ] *(Optional)* `WinrtCapture` implementation (deferred)
- [ ] *(Optional)* `DMLDetector` implementation (deferred to Phase 3 if needed)
- [ ] *(Optional)* `TensorRTDetector` (NVIDIA Backend, deferred)

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
- [2025-12-30] **Phase 2 Capture & Detection Complete** (Commits: b5d3af2 through 417c0dc)
  - TexturePool with RAII TextureHandle (prevents resource leaks)
  - DuplicationCapture with zero-copy GPU optimization
  - Input preprocessing compute shader (BGRA→RGB tensor conversion)
  - NMS post-processing (IoU-based overlap removal, confidence filtering, hitbox mapping)
  - Unit tests for TexturePool and PostProcessor (12 test cases, 1095 assertions)
  - Build system integration (CMake for capture/detection modules, HLSL shader compilation)
  - Critical Trap #1 addressed: RAII deleter prevents texture pool starvation
  - Completion report: docs/phase2-completion.md
- [2025-12-30] **Phase 1 Foundation Complete** (Commit: 02db714)
  - Namespace rename (sunone → macroman)
  - Lock-free LatestFrameQueue implementation
  - ThreadManager with Windows priorities
  - spdlog logging system
  - Catch2 unit testing framework
  - Core interfaces audited
  - Build system functional
- [2025-12-30] Standardized all 10 implementation plans
- [2025-12-30] Defined State-Action-Sync workflow in `CLAUDE.md`
- [2025-12-30] Initial architectural audit and backlog creation
