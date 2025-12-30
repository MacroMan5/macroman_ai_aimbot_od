# Architecture Quick Reference

This document serves as a map for developers and AI agents to navigate the codebase efficiently.

## 1. The Core Pipeline (Data Flow)

```mermaid
graph LR
    A[Capture Thread] --"Frame (Texture*)"--> B[Detection Thread]
    B --"DetectionBatch"--> C[Tracking Thread]
    C --"AimCommand (Atomic)"--> D[Input Thread]
    
    subgraph Shared Resources
    TP[Texture Pool]
    DB[Target Database (SoA)]
    CFG[Shared Config (IPC)]
    end
    
    A -.-> TP
    B -.-> TP
    C -.-> DB
    D -.-> CFG
```

## 2. Directory Structure & Responsibilities

| Directory | Component | Responsibility | Key Classes |
|-----------|-----------|----------------|-------------|
| **`src/core`** | **Core** | Interfaces, Types, Threading primitives | `IScreenCapture`, `IDetector`, `Frame`, `TargetDB` |
| **`src/capture`** | **Capture** | Getting images from screen | `WinrtCapture`, `DuplicationCapture` |
| **`src/detection`** | **Detection** | Running AI models | `DMLDetector`, `TensorRTDetector`, `PostProcessor` |
| **`src/input`** | **Input** | Mouse movement & trajectories | `TrajectoryPlanner`, `Win32Driver`, `ArduinoDriver` |
| **`src/tracking`** | **Tracking** | Target state and prediction | `KalmanPredictor` |
| **`src/ui`** | **UI** | Overlay & Menus | `OverlayWindow`, `DebugPanel` |
| **`src/config`** | **Config** | JSON/INI parsing | `AppConfig`, `GameProfile` |

## 3. Key Data Structures

### `Frame` (`src/core/entities/Frame.h`)
*   **What:** Represents a single captured moment.
*   **Lifecycle:** Allocated from `FrameAllocator`, holds `Texture*` from `TexturePool`. Passed to Detection, then dropped.
*   **Crucial Fields:** `captureTime` (for latency calc), `texture` (GPU pointer).

### `TargetDatabase` (`src/core/entities/TargetDatabase.h`)
*   **What:** The "World State" of enemies.
*   **Storage:** Structure-of-Arrays (SoA). `std::array<float, 64> x`, `std::array<float, 64> y`, etc.
*   **Owner:** Tracking Thread (Write), Overlay (Read-Snapshot).

### `AimCommand` (`src/core/entities/AimCommand.h`)
*   **What:** The instruction for the mouse.
*   **Mechanism:** `std::atomic<AimCommand>` shared between Tracking and Input.
*   **Fields:** `deltaX`, `deltaY`, `shouldFire`.

## 4. The "Hot Path" Rules
Code in `Capture`, `Detection`, and `Input` threads must adhere to:
1.  **No syscalls** (File I/O, `cout`).
2.  **No heap allocation** (No `std::vector` resizing).
3.  **No locks** (Use `TryDequeue` or atomic exchange).

## 5. Build & Test
*   **Build:** `cmake --build build`
*   **Test:** `ctest --test-dir build`
*   **Bench:** `sunone-bench.exe` (Post-build)
