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

## Phase 3: Tracking & Prediction ✅ **COMPLETE**
- [x] Implement `TargetDatabase` (SoA structure with 64 max targets)
- [x] Implement `DetectionBatch` (FixedCapacityVector for zero heap allocations)
- [x] Implement `AimCommand` structure (atomic inter-thread communication)
- [x] Implement `MathTypes` module (Vec2, BBox, KalmanState primitives)
- [x] Implement `KalmanPredictor` (stateless 4-state predictor)
- [x] Implement `DataAssociation` (greedy IoU matching algorithm)
- [x] Implement `TargetSelector` (FOV + distance + hitbox priority)
- [x] Implement `TargetTracker` (grace period maintenance, 100ms coast)
- [x] Implement unified timestamping system across pipeline
- [x] Unit tests for tracking components (8 test files, ~500 lines)
- [x] Build system integration (CMake for tracking module)
- [x] Critical Trap #5 addressed (FixedCapacityVector prevents heap allocations)

## Phase 4: Input & Aiming ✅ **100% COMPLETE**
- [x] Implement `InputManager` (1000Hz loop orchestration with timing variance)
- [x] Implement `Win32Driver` (SendInput API, <1ms latency)
- [x] Implement `ArduinoDriver` (Serial HID, 200 lines, optional - requires libserial)
- [x] Implement `TrajectoryPlanner` (Bezier curve support with smoothness parameter)
- [x] Implement `BezierCurve` (overshoot simulation: 15% past target)
- [x] Implement `OneEuroFilter` (adaptive low-pass filtering)
- [x] Implement `Humanizer` module (reaction delay + micro-tremor)
- [x] Implement Deadman Switch safety mechanism (200ms timeout)
- [x] Implement Emergency Shutdown (1000ms timeout)
- [x] Implement Timing Variance (±20% jitter: 800-1200Hz actual)
- [x] Unit tests for input components (34 tests: 15 humanization + 19 integration)
- [x] Build system integration (CMake for input module with ENABLE_ARDUINO option)
- [x] Critical Traps #2 & #4 addressed (prediction clamping + deadman switch)
- [x] Documentation (docs/ARDUINO_SETUP.md for ArduinoDriver setup)

## Phase 5: Configuration & Auto-Detection ⚪
- [ ] Implement Game Profile JSON system
- [ ] Implement `GameDetector` (Auto-detection with hysteresis)
- [ ] Implement `SharedConfig` (Memory-mapped IPC for live tuning) - **Note:** Base implementation exists in src/core/config/
- [ ] Implement `SharedConfigManager` (Windows memory-mapped file wrapper)
- [ ] Implement ModelManager (Thread-safe model switching)
- [ ] Unit tests for config components

## Phase 6: UI & Observability ✅ **COMPLETE**
- [x] In-game overlay (ImGui transparent window - 800x600, frameless)
- [x] Performance metrics display (FPS, latency breakdown, VRAM usage)
- [x] Bounding box visualization (color-coded by hitbox type + selected target)
- [x] Component toggles (enable/disable tracking, prediction, aiming, tremor via SharedConfig)
- [x] SetWindowDisplayAffinity screenshot protection (WDA_EXCLUDEFROMCAPTURE)
- [x] Safety metrics panel (Critical Traps monitoring)
- [x] External config UI (macroman_config.exe standalone ImGui app, 1200x800)
- [x] Live tuning sliders (smoothness, FOV via SharedConfig IPC)
- [x] Telemetry dashboard (real-time metrics from SharedConfig)
- [x] FrameProfiler implementation (300-sample ring buffer, latency graphs)
- [x] BottleneckDetector (stage-specific performance suggestions integrated into FrameProfiler)
- [x] Build system integration (CMakeLists.txt for ui/ and config_app/)
- [ ] Unit tests for UI components (pending - requires Engine integration for ImGui context)
- [ ] Verify overlay doesn't impact game performance (<1ms overhead - requires full Engine integration)

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

---

## ✅ Completed Tasks
- [2025-12-31] **Phase 6 UI & Observability Complete** (Commits: TBD)
  - DebugOverlay implementation (800x600 transparent ImGui window)
  - Performance metrics panel (FPS, latency breakdown, VRAM usage with color coding)
  - Bounding box visualization (colored by hitbox: RED=head, ORANGE=chest, YELLOW=body, GREEN=selected)
  - Component toggles panel (runtime enable/disable for tracking, prediction, aiming, tremor)
  - SetWindowDisplayAffinity screenshot protection (WDA_EXCLUDEFROMCAPTURE for Windows 10 1903+)
  - Safety metrics panel (Critical Traps monitoring: texture pool starvation, stale predictions, deadman switch)
  - FrameProfiler implementation (300-sample ring buffer history, 5 seconds @ 60fps UI refresh)
  - Latency graphs (4 line graphs: capture, detection, tracking, input with P95 thresholds)
  - Bottleneck detection with stage-specific suggestions (Detection→reduce input size/TensorRT, Capture→GPU busy/reduce graphics, etc.)
  - External Config UI (macroman_config.exe - 1200x800 standalone ImGui application)
  - Live tuning sliders (smoothness 0.0-1.0, FOV 10-180 degrees via SharedConfig IPC)
  - Telemetry dashboard (real-time metrics from SharedConfig atomics)
  - Component toggles in config UI (read/write SharedConfig atomics for enable flags)
  - Build system integration (src/ui/CMakeLists.txt, src/ui/config_app/CMakeLists.txt with GLOB_RECURSE)
  - Fixed namespace closure bug in ConfigApp.cpp (WndProc in global namespace)
  - Fixed HISTORY_SIZE declaration order in FrameProfiler.h (static constexpr moved to public section)
  - Unit tests: Pending (requires Engine integration for ImGui context)
  - Performance verification: Pending (requires full Engine integration)
  - Completion report: TBD (will be created before PR merge)
- [2025-12-30] **Phase 4 Input & Aiming 100% Complete** (Commits: 5f7e6d4 through bac78c3)
  - InputManager with 1000Hz loop orchestration (actual: 800-1200Hz with timing variance)
  - Win32Driver using SendInput API (<1ms latency)
  - ArduinoDriver fully implemented (200 lines, optional - requires libserial + Arduino hardware)
  - TrajectoryPlanner with Bezier curve support and smoothness parameter
  - BezierCurve with overshoot simulation (15% past target)
  - OneEuroFilter adaptive low-pass filtering
  - Humanizer module (reaction delay 100-300ms + micro-tremor 10Hz, 0.5px amplitude)
  - Deadman switch safety mechanism (200ms timeout, 1000ms emergency shutdown)
  - Timing variance (±20% jitter to avoid superhuman consistency)
  - Unit tests: 34 tests (15 humanization + 19 integration), 1612 total test lines
  - Build system integration (CMake for input module with ENABLE_ARDUINO option)
  - Critical Traps #2 & #4 addressed (prediction clamping + deadman switch)
  - Performance: <10ms end-to-end latency (capture → mouse movement)
  - ArduinoDriver: Enable with -DENABLE_ARDUINO=ON (see docs/ARDUINO_SETUP.md)
  - Completion report: docs/phase4-completion.md
- [2025-12-30] **Phase 3 Tracking & Prediction Complete** (Commits: 80d0973 through 70eec43)
  - TargetDatabase with Structure-of-Arrays (SoA) for 64 max targets
  - DetectionBatch with FixedCapacityVector (zero heap allocations in hot path)
  - AimCommand structure for Tracking→Input thread communication (64-byte aligned, atomic)
  - MathTypes module (Vec2, BBox, KalmanState primitives)
  - KalmanPredictor (stateless 4-state: [x, y, vx, vy])
  - DataAssociation (greedy IoU matching algorithm)
  - TargetSelector (FOV + distance + hitbox priority)
  - TargetTracker (grace period maintenance, 100ms coast)
  - Unit tests: 8 test files, ~500 lines of test code
  - Build system integration (CMake for tracking module)
  - Critical Trap #5 addressed (FixedCapacityVector prevents texture pool starvation)
  - Performance: <1ms tracking latency
  - Completion report: docs/phase3-completion.md
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
