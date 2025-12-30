# Phase 6: Safety & Humanization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement behavioral humanization to make the aimbot indistinguishable from a skilled human player and add screenshot protection for the overlay.

**Architecture:** Integrate human motor control imperfections into the Input Thread. Use normal distributions for reaction times and sinusoidal functions for physiological tremor. Implement Windows Display Affinity for overlay privacy.

**Tech Stack:** C++20, Windows User32 API, `<random>`, `<cmath>`.

**Reference:** `docs/plans/2025-12-29-modern-aimbot-architecture-design.md` Section 10.

---

## Task P6-01: Reaction Delay Manager

**Goal:** Prevent "superhuman" instant reactions to visual stimuli.

**Files:**
- Create: `src/core/humanization/ReactionDelayManager.h/cpp`
- Modify: `src/core/CMakeLists.txt`

**Step 1: Implement Delay Logic**
Create a manager that uses `std::normal_distribution` (μ=160ms, σ=25ms) to buffer new target acquisitions.

**Step 2: Verification**
Unit test that delays fall within the 100ms - 300ms range.

---

## Task P6-02: Physiological Tremor Simulator

**Goal:** Add natural 8-12Hz micro-jitter to mouse movement.

**Files:**
- Create: `src/core/humanization/HumanTremorSimulator.h/cpp`

**Step 1: Implement Tremor**
Add a sinusoidal jitter (0.5px amplitude) to the `AimCommand` delta before sending to the driver.

**Step 2: Verification**
Unit test that deltas are modified but average out to zero over 1 second.

---

## Task P6-03: Bezier Overshoot & Correction

**Goal:** Mimic human "flick" mechanics where the crosshair slightly passes the target.

**Files:**
- Modify: `src/input/movement/BezierCurve.cpp`

**Step 1: Implement Overshoot**
Update the curve generation to extend 15% past the target and then pull back in the final 50ms.

**Step 2: Verification**
Trace the path in unit tests to confirm $t > 1.0$ occurs before returning to $1.0$.

---

## Task P6-04: Overlay Privacy (Windows Display Affinity)

**Goal:** Make the ImGui overlay invisible to OBS, Discord, and anti-cheat screenshots.

**Files:**
- Modify: `src/ui/ImGuiContext.cpp`

**Step 1: Apply Affinity**
Call `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` during window initialization.

**Step 2: Verification**
Manual check using Windows Snipping Tool (overlay should be black or invisible in the snip).

---

## Deliverables
- [ ] `ReactionDelayManager` integrated into Tracking thread.
- [ ] `HumanTremorSimulator` integrated into Input thread.
- [ ] Updated `BezierCurve` with flick mechanics.
- [ ] Capture-proof debug overlay.
