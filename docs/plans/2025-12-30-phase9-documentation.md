# Phase 9: Documentation Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Deliver comprehensive technical and user documentation for the MacroMan AI Aimbot v2. This phase ensures the project is maintainable for developers and usable for educational testers by providing a clear technical deep-dive and operational instructions.

**Architecture:** Use Doxygen for automated API extraction. Markdown for human-readable guides. Integrated into CMake build system for documentation-as-code.

**Tech Stack:** Doxygen 1.9+, Graphviz (for diagrams), Markdown, CMake.

**Reference:** `docs/plans/2025-12-29-modern-aimbot-architecture-design.md`

---

## Task P9-01: Doxygen Infrastructure Setup

**Files:**
- Create: `docs/Doxyfile.in`
- Modify: `CMakeLists.txt`

**Step 1: Create Doxygen template**
Create `docs/Doxyfile.in` with project-specific settings, including paths to `src` and `extracted_modules`.

**Step 2: Update CMake for Doxygen target**
Add `find_package(Doxygen)` and `doxygen_add_docs` to the root `CMakeLists.txt` to allow building documentation via `cmake --build build --target docs`.

**Step 3: Verify generation**
Run: `cmake --build build --target docs`
Expected: `build/docs/html/index.html` is created.

**Step 4: Commit**
```bash
git add docs/Doxyfile.in CMakeLists.txt
git commit -m "docs: setup Doxygen infrastructure"
```

---

## Task P9-02: Core Interface Documentation Audit

**Files:**
- Modify: `extracted_modules/core/interfaces/*.h`
- Modify: `extracted_modules/core/entities/*.h`

**Step 1: Standardize Header Comments**
Apply Javadoc-style comments to all pure virtual methods in `IScreenCapture`, `IDetector`, and `IMouseDriver`.

**Step 2: Document Threading Guarantees**
Add `@note Thread Safety` sections to each interface detailing which thread owns the component (Capture, Detection, Tracking, or Input).

**Step 3: Commit**
```bash
git add extracted_modules/core/
git commit -m "docs: audit core interfaces and entities for API documentation"
```

---

## Task P9-03: Technical Developer Guide

**Files:**
- Create: `docs/DEVELOPER_GUIDE.md`

**Step 1: Document Pipeline Orchestration**
Describe the 4-thread pipeline and the "Head-Drop" policy implementation in detail.

**Step 2: Document Memory Strategy**
Explain the zero-allocation hot path and the `TexturePool` RAII logic.

**Step 3: Commit**
```bash
git add docs/DEVELOPER_GUIDE.md
git commit -m "docs: add technical developer guide"
```

---

## Task P9-04: User Setup & Calibration Guide

**Files:**
- Create: `docs/USER_GUIDE.md`

**Step 1: Write Prerequisites & Installation**
Detail Windows version requirements and GPU driver dependencies.

**Step 2: Write Calibration Instructions**
Explain how to tune FOV, Smoothness, and the 1 Euro Filter for specific gaming environments.

**Step 3: Commit**
```bash
git add docs/USER_GUIDE.md
git commit -m "docs: add user setup and calibration guide"
```

---

## Task P9-05: Safety, Ethics, & Humanization Manual

**Files:**
- Create: `docs/SAFETY_ETHICS.md`

**Step 1: Document Humanization Features**
Detail the logic behind `ReactionDelayManager`, `HumanTremorSimulator`, and `Bezier Overshoot`.

**Step 2: Write Legal Disclaimer**
Reinforce the educational-only nature of the project and prohibited use cases.

**Step 3: Commit**
```bash
git add docs/SAFETY_ETHICS.md
git commit -m "docs: add safety and ethics manual"
```

---

## Task P9-06: Troubleshooting & FAQ

**Files:**
- Create: `docs/FAQ.md`

**Step 1: Document Common Implementation Traps**
Address issues like "Texture Pool Starvation" and "Input Latency Spikes".

**Step 2: Component-Specific FAQ**
Provide solutions for DirectML vs TensorRT issues and WinRT capture permissions.

**Step 3: Commit**
```bash
git add docs/FAQ.md
git commit -m "docs: add troubleshooting and FAQ guide"
```

---

## Deliverables
- [ ] `docs/api/html/index.html` (Generated Doxygen Site)
- [ ] `docs/USER_GUIDE.md`
- [ ] `docs/DEVELOPER_GUIDE.md`
- [ ] `docs/SAFETY_ETHICS.md`
- [ ] `docs/FAQ.md`
- [ ] Doxygen integration in `CMakeLists.txt`
