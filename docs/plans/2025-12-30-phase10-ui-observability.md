# Phase 10: UI & Observability Implementation Plan

> **Status:** 🟡 In Progress (Core Implemented, Polish Remaining)
> **Architecture:** Native D3D11 + Win32 (No GLFW)

**Goal:** Refine the in-game debug overlay and external config UI to be production-ready, ensuring low overhead (<1ms) and robust input handling.

**Architecture:**
- **Overlay:** Native Win32 Layered Window (Transparent) + D3D11 SwapChain + ImGui.
- **Config App:** Standalone Win32 Window + D3D11 + ImGui.
- **IPC:** Shared Memory (Memory Mapped File) for zero-copy telemetry and config tuning.
- **Rendering:** Direct3D 11 (matches Capture pipeline).

**Tech Stack:** Dear ImGui 1.91.2, Direct3D 11, Windows API (User32), C++20.

**Reference:** `src/ui/backend/D3D11Backend.cpp`, `src/Engine.cpp`

---

## 1. Audit & Reconciliation

| Feature | Plan (Original) | Implementation (Actual) | Status |
| :--- | :--- | :--- | :--- |
| **Windowing** | GLFW 3.4 | Native Win32 (`CreateWindowEx`) | ✅ Better integration |
| **Rendering** | OpenGL 3.3 | Direct3D 11 | ✅ Matches Capture |
| **Overlay** | `ImGuiContext` wrapper | `DebugOverlay` + `D3D11Backend` | ✅ Implemented in `src/ui/overlay` |
| **Widgets** | `src/ui/widgets/` | `src/ui/overlay/FrameProfiler.h` | ✅ Moved to overlay |
| **IPC** | Shared Memory | `SharedConfigManager` | ✅ Implemented in `src/core/config` |
| **Demo App** | `macroman_ui_demo` | *Not Implemented* | 🟡 Todo |

**Decision:** Stick with **Native D3D11**. It removes the GLFW dependency and allows sharing D3D devices with the Capture module if needed in the future (Zero-Copy 2.0).

---

## 2. Remaining Tasks (Phase 10)

### Task P10-01: UI Polish & Theming
**Goal:** Make the UI look professional (Cyberpunk/Dark theme).
- [ ] Implement `ImGuiBackend::setTheme()` in `src/ui/backend/ImGuiBackend.cpp` with custom colors (Dark Grey/Green/Red).
- [ ] Load custom fonts (e.g., Roboto/JetBrains Mono) instead of default ProggyClean.
- [ ] Add tooltips to all Config App sliders.

### Task P10-02: Input Passthrough Toggle
**Goal:** Allow users to interact with the game *through* the overlay.
- [ ] Modify `src/Engine.cpp` `WndProc` or input loop.
- [ ] Add hotkey (e.g., `INSERT`) to toggle Overlay interaction.
- [ ] When locked: `SetWindowLong(GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOPMOST)`.
- [ ] When unlocked: Remove `WS_EX_TRANSPARENT` to allow ImGui interaction.

### Task P10-03: High-DPI Scaling Support
**Goal:** Prevent UI from looking tiny on 1440p/4K monitors.
- [ ] Call `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` in `src/Engine.cpp` and `src/ui/config_app/ConfigApp.cpp`.
- [ ] Handle `WM_DPICHANGED` in `WndProc`.
- [ ] Scale ImGui style sizes by DPI factor.

### Task P10-04: Standalone UI Test (Demo App)
**Goal:** Verify UI rendering without starting the full Engine/Detection pipeline.
- [ ] Create `src/ui_demo_main.cpp`.
- [ ] Instantiate `D3D11Backend`, `ImGuiBackend`, `DebugOverlay`.
- [ ] Feed fake telemetry data (sine waves) to verify graphs.
- [ ] Add to `CMakeLists.txt` as `macroman_ui_demo`.

### Task P10-05: Safety Metrics Visualization
**Goal:** Ensure "Critical Traps" are visible.
- [ ] Verify `DebugOverlay::renderSafetyAlerts()` in `src/ui/overlay/DebugOverlay.cpp` triggers correctly.
- [ ] Add visual "Flash" (red border) when a trap is triggered (e.g., Deadman Switch).

---

## 3. Blockers / Risks

1.  **Input Stealing:** If Overlay takes focus, game input might stop.
    *   *Mitigation:* Use `WS_EX_NOACTIVATE` and ensure `SetFocus` remains on game window unless Menu Mode is active.
2.  **Performance:** ImGui overhead must stay <0.5ms.
    *   *Mitigation:* Profile `DebugOverlay::render()` with Tracy. Avoid complex widgets in hot path.
3.  **Fullscreen Exclusive:** Overlay cannot draw over Fullscreen Exclusive games.
    *   *Mitigation:* User must use Borderless Windowed mode. (Document this limitation).

---

## 4. Execution Plan

1.  **P10-04 (Demo App):** Implement first to iterate on UI fast.
2.  **P10-01 (Theming):** Apply style updates.
3.  **P10-02 (Passthrough):** Implement interaction toggle in Engine.
4.  **P10-03 (DPI):** Fix scaling issues.
5.  **Validation:** Verify against `macroman_aimbot.exe` (Engine).
