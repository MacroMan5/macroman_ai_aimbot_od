# Phase 4: Input & Aiming Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement smooth, human-like mouse control with dynamic trajectory planning, behavioral humanization, and critical safety mechanisms. Deliverable: End-to-end aimbot functionality (capture → move) with <10ms latency.

**Architecture:** 1000Hz Input Thread orchestrating a dynamic planning loop. TrajectoryPlanner handles moving targets via Bezier curves with 15% overshoot/correction. Safety layer includes a 200ms Deadman Switch and 50ms Extrapolation Clamping.

**Tech Stack:** C++20, Windows SendInput API, Serial (Arduino HID), MathTypes (from Phase 3), 1 Euro Filter

**Reference:** `docs/plans/2025-12-29-modern-aimbot-architecture-design.md` Sections 6.4 (Input Module), 10 (Safety & Humanization), 11 (Critical Risks)

---

## Task P4-01: Win32Driver Implementation (Testing)

**Files:**
- Create: `src/input/drivers/Win32Driver.h`
- Create: `src/input/drivers/Win32Driver.cpp`
- Create: `src/input/CMakeLists.txt`

**Step 1: Implement Win32Driver**
Use `SendInput` API for development and testing. Implement `initialize()`, `move(dx, dy)`, and `shutdown()`.

**Step 2: Setup Input Module CMake**
Create `src/input/CMakeLists.txt` and link to `macroman_core`.

---

## Task P4-02: ArduinoDriver Implementation (Production)

**Files:**
- Create: `src/input/drivers/ArduinoDriver.h`
- Create: `src/input/drivers/ArduinoDriver.cpp`

**Step 1: Define Serial Protocol**
Simple binary protocol: `[0xAA][deltaX_low][deltaX_high][deltaY_low][deltaY_high][Checksum]`.

**Step 2: Implement Driver**
Use Win32 Serial API (CreateFile/WriteFile) to send movement packets to an external HID emulator.

---

## Task P4-03: Bezier Curve with Overshoot & Correction

**Files:**
- Create: `src/input/movement/BezierCurve.h`
- Create: `tests/unit/test_bezier.cpp`

**Step 1: Implement Cubic Bezier**
Implement `evaluate(t)` for progress $t \in [0, 1.15]$.

**Step 2: Add Overshoot Logic**
Modify evaluation to handle $t > 1.0$ (overshoot phase) and interpolate back to $1.0$ during the "correction" phase ($t \in [1.0, 1.15]$) as defined in Design Doc 10.1.3.

---

## Task P4-04: 1 Euro Filter Implementation

**Files:**
- Create: `src/input/movement/OneEuroFilter.h`
- Create: `src/input/movement/OneEuroFilter.cpp`
- Create: `tests/unit/test_one_euro_filter.cpp`

**Step 1: Implement Adaptive Low-Pass Filter**
Implement `filter(val, dt)` with tunable `minCutoff` and `beta`. This reduces jitter while maintaining flick responsiveness.

---

## Task P4-05: Humanization - Reaction & Tremor

**Files:**
- Create: `src/input/humanization/Humanizer.h/cpp`
- Create: `tests/unit/test_humanizer.cpp`

**Step 1: Reaction Delay Manager**
Implement Normal Distribution (μ=160ms, σ=25ms) delay for new target acquisitions.

**Step 2: Physiological Tremor Simulator**
Implement 10Hz sinusoidal micro-jitter (0.5px amplitude) to be added to every `step()`.

---

## Task P4-06: Dynamic Trajectory Planner

**Files:**
- Create: `src/input/movement/TrajectoryPlanner.h/cpp`

**Step 1: Refactor for Moving Targets**
`step(AimCommand cmd, float dt)` must:
1.  Extrapolate `cmd.targetPosition` using `cmd.targetVelocity`.
2.  Update the Bezier curve if the target has moved significantly.
3.  Evaluate the curve and apply the 1 Euro filter.

---

## Task P4-07: Input Loop & Safety Mechanisms

**Files:**
- Create: `src/input/InputManager.h/cpp` (Orchestrator)

**Step 1: Implement 1000Hz Loop**
High-priority thread performing: `Atomic Load AimCommand` -> `Trajectory Step` -> `Driver Move`.

**Step 2: Deadman Switch (Risk 11.4)**
Stop all mouse movement if `latestCommand` timestamp is >200ms old.

**Step 3: Extrapolation Clamping (Risk 11.2)**
Cap prediction `dt` at 50ms. If detection lags, stop aiming and decay confidence to prevent "screen-edge snapping".

---

## Task P4-08: Phase 4 Integration Test

**Files:**
- Create: `tests/integration/test_input_pipeline.cpp`

**Step 1: Verified End-to-End**
Mock `IDetector` and `IScreenCapture` to feed a moving target. Verify `IMouseDriver` receives smooth, overshoot-corrected deltas.

---

## Execution Handoff

Plan complete. Critical risks from Architecture Section 11 (Deadman, Extrapolation) are now first-class tasks. Math entities are consolidated into `MathTypes.h`.

Ready for execution.