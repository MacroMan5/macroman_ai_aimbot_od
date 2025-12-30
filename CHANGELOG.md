# Changelog

All notable changes to the MacroMan AI Aimbot project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Phase 2: Capture & Detection - 2025-12-30

#### Added
- **TexturePool** - GPU texture pool with RAII resource management
  - Triple-buffer design (3 textures) for zero-copy GPU pipeline
  - `TextureHandle` with custom `TextureDeleter` for automatic cleanup
  - Prevents "Leak on Drop" trap (Critical Trap #1 from `docs/CRITICAL_TRAPS.md`)
  - Starvation metrics for pool exhaustion tracking
  - Thread-safe acquire/release with mutex
  - Comprehensive unit tests (5 test cases)

- **DuplicationCapture** - Desktop Duplication API implementation
  - Captures desktop at 120+ FPS with <1ms latency
  - Zero-copy GPU-to-GPU copy using `CopySubresourceRegion`
  - Automatic cropping to game window region (centered square)
  - Error recovery for `DXGI_ERROR_ACCESS_LOST` (reinitializes on desktop mode change)
  - Head-drop policy when texture pool is starved
  - Non-blocking capture (0ms timeout)

- **InputPreprocessor** - HLSL compute shader for tensor preprocessing
  - Converts BGRA texture → RGB planar normalized tensor (NCHW layout)
  - 8x8 thread group dispatch for 640x640 textures (80x80 groups)
  - Fills "Tensor Gap" between screen capture and AI inference
  - Nearest-neighbor sampling (optimized for speed)
  - Auto-compiles `.hlsl` → `.cso` via CMake custom command

- **PostProcessor** - NMS and post-processing utilities
  - Non-Maximum Suppression (greedy algorithm, IoU threshold: 0.45)
  - Confidence filtering (default threshold: 0.6)
  - Hitbox mapping (classId → HitboxType enum)
  - In-place modification for zero allocations in hot path
  - Unit tests for NMS, confidence filtering, hitbox mapping (3 test cases)

- **Build System Integration**
  - Capture module CMake with WinRT support and D3D11/DXGI linking
  - Detection module CMake with ONNX Runtime 1.22.0 integration
  - Auto-copy `onnxruntime.dll` to output directory
  - HLSL shader compilation via `fxc.exe`
  - Unit test framework integration (Catch2 v3)

- **Documentation**
  - `docs/phase2-completion.md` - Comprehensive Phase 2 completion report
  - Updated `docs/plans/State-Action-Sync/STATUS.md` - Current progress (40%)
  - Updated `docs/plans/State-Action-Sync/BACKLOG.md` - Phase 2 marked complete

#### Changed
- **Frame entity** - Updated to use `TextureHandle` instead of raw pointer
  - Move-only semantics (deleted copy constructor/assignment)
  - RAII automatic cleanup on destruction
  - Prevents resource leaks in `LatestFrameQueue` head-drop policy

- **Detection entity** - Updated for PostProcessor integration
  - Added `BBox` struct with `area()` method
  - Added `HitboxType` enum (Unknown, Head, Chest, Body)
  - Constructor prevents aggregate initialization (explicit member assignment required)

#### Fixed
- Critical Trap #1 "Leak on Drop" - TexturePool now uses RAII deleter
  - Frame destructor automatically releases texture back to pool
  - Prevents pool starvation when frames are dropped by `LatestFrameQueue`
  - Validated via unit tests (starvation metric tracking)

#### Performance
- Zero-copy GPU pipeline - No staging textures, direct GPU-to-GPU copy
- Triple buffer prevents blocking - Capture thread never waits for detector
- Head-drop policy - Always process latest frame (real-time priority)
- All unit tests passing - 12 test cases, 1095 assertions (Release x64)

#### Known Limitations
- Optional components deferred to future phases:
  - `DMLDetector` implementation (can be added in Phase 3 if needed)
  - `WinrtCapture` implementation (better FPS than DuplicationCapture)
  - `TensorRTDetector` implementation (NVIDIA-only optimization)

---

### Phase 1: Foundation - 2025-12-30

#### Added
- **Core Interfaces** - Interface-based architecture
  - `IScreenCapture` - Screen capture interface
  - `IDetector` - AI detection interface
  - `IMouseDriver` - Mouse input interface

- **Threading Utilities**
  - `LatestFrameQueue` - Lock-free head-drop queue for real-time pipelines
  - `ThreadManager` - Windows-specific thread priority management

- **Logging System**
  - spdlog integration with dual sinks (console + rotating file)
  - Structured logging with severity levels

- **Testing Framework**
  - Catch2 v3 integration
  - CMake CTest integration
  - Initial unit tests for `LatestFrameQueue` (2 test cases)

#### Changed
- **Global Namespace** - Renamed from `sunone` to `macroman`
  - Updated all headers, source files, and documentation
  - Consistent naming across entire codebase

#### Architecture
- CMake 3.25+ build system with modular structure
- C++20 standard enabled throughout project
- Windows 10/11 (1903+) target platform
- MSVC 2022 compiler

---

## Project Information

**Repository:** https://github.com/yourusername/macroman_ai_aimbot
**License:** TBD
**Platform:** Windows 10/11 (1903+)
**Compiler:** MSVC 2022, C++20

For detailed implementation plans, see `docs/plans/2025-12-29-modern-aimbot-architecture-design.md`
