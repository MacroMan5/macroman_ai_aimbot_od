# Contributing to Sunone Aimbot (V2 Refactor)

Welcome to the development team. This project is a high-performance, external AI aimbot written in C++20. We prioritize **low latency (<10ms)**, **memory safety**, and **anti-cheat invisibility**.

## 🛠️ Build Environment

**Strict Requirements:**
- **OS:** Windows 10/11 (1903+)
- **Compiler:** MSVC (Visual Studio 2022 v17.x+)
- **Standard:** C++20 (`/std:c++20`)
- **Build System:** CMake 3.25+
- **SDKs:** 
  - CUDA Toolkit 12.8+ (for TensorRT)
  - Windows SDK 10.0.22621+

### 🚀 Build Commands

```powershell
# 1. Configure (Generate VS Solution)
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. Build (Multicore)
cmake --build build --config RelWithDebInfo --parallel

# 3. Run Tests
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

---

## 📐 Coding Standards (Strict)

We use **Modern C++20**. Legacy C++ patterns are forbidden.

### 1. Memory Management
*   **❌ BANNED:** `new`, `delete`, `malloc`, `free`.
*   **✅ REQUIRED:** `std::unique_ptr`, `std::shared_ptr` (sparingly), `std::vector`.
*   **✅ OBJECT POOLS:** For hot-path objects (`Frame`, `DetectionBatch`), use the `ObjectPool` or `TexturePool`. **Never allocate heap memory in the render/capture loop.**

### 2. Concurrency
*   **❌ BANNED:** `std::mutex` in the hot path (Capture/Detect/Aim).
*   **✅ REQUIRED:** `moodycamel::ConcurrentQueue` for pipeline communication.
*   **✅ REQUIRED:** `std::atomic` for flags and counters.
*   **THREAD SAFETY:** Assume all code runs in a multithreaded environment. Use `const` correctness religiously.

### 3. Style Guide
*   **Formatter:** We use `.clang-format` (Google Style based).
*   **Naming:**
    *   Types/Classes: `PascalCase` (`TargetDatabase`, `Frame`)
    *   Functions/Methods: `camelCase` (`updatePredictions`, `captureFrame`)
    *   Variables: `camelCase` (`localVariable`)
    *   Private Members: `m_camelCase` (`m_isValid`)
    *   Constants: `kPascalCase` or `SCREAMING_SNAKE_CASE` (`kMaxTargets`, `MAX_TARGETS`)

### 4. Safety & Anti-Cheat
*   **READ-ONLY:** Never open a handle to the game process with Write permissions.
*   **NO INJECTION:** Never inject DLLs into the game.
*   **OVERLAY:** Always ensure `SetWindowDisplayAffinity` is used for the overlay.

---

## 🌳 Git Workflow

We use a **Feature Branch** workflow.

1.  **`main`**: Production-ready code.
2.  **`dev`**: Integration branch. **All PRs target `dev`.**
3.  **`feat/name`**: New features.
4.  **`fix/name`**: Bug fixes.
5.  **`perf/name`**: Performance optimizations.

### Commit Messages (Conventional Commits)
*   `feat: add DirectX11 texture pool`
*   `fix: resolve race condition in input thread`
*   `perf: switch to AVX2 for target sorting`
*   `chore: update .gitignore`
*   `docs: update architecture diagram`

---

## 🧪 Definition of Done

A task is not done until:
1.  It compiles without warnings (`/W4`).
2.  Unit tests (Catch2) pass.
3.  It does not introduce memory leaks (verified mentally or via tool).
4.  If it touches the hot path, latency impact is verified.
