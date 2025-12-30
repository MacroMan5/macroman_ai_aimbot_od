# Phase 0: Architecture Setup & Foundation

**Status:** Planned
**Priority:** Critical (Prerequisite for all other phases)
**Goal:** Establish the clean project structure, build system, and migrate `extracted_modules` into a production-ready C++ architecture.

---

## 1. Directory Structure Definition

We will transition from the flat `extracted_modules` layout to a standard, scalable C++ project structure.

```text
marcoman_ai_aimbot/
├── CMakeLists.txt              # Root build configuration
├── CMakePresets.json           # Build presets (Windows/Ninja/VS)
├── assets/                     # Runtime assets (models, images)
│   ├── models/
│   └── icons/
├── cmake/                      # CMake modules and helper scripts
│   ├── FindDirectX.cmake
│   ├── FindTensorRT.cmake
│   └── CopyDlls.cmake
├── external/                   # Third-party libraries (ImGui, etc.)
│   ├── imgui/
│   ├── json/
│   └── simpleini/
├── src/                        # Source code root
│   ├── main.cpp                # Application entry point
│   ├── core/                   # Shared types, interfaces, utils
│   │   ├── entities/
│   │   ├── interfaces/
│   │   └── threading/
│   ├── capture/                # Screen capture implementations
│   ├── detection/              # AI inference engines
│   │   ├── directml/
│   │   └── tensorrt/
│   ├── input/                  # Mouse injection & drivers
│   ├── tracking/               # Kalman filter & path planning
│   ├── config/                 # Configuration management
│   └── ui/                     # Overlay and GUI
└── tests/                      # Unit and integration tests
```

## 2. Migration Strategy (Extracted -> Src)

We will copy and refactor code from `extracted_modules` into `src`. This is not a drag-and-drop; we will verify namespaces and include paths during the move.

| Source (`extracted_modules/`) | Destination (`src/`) | Action |
|-------------------------------|----------------------|--------|
| `core/` | `src/core/` | Copy interfaces, entities, utils. |
| `capture/` | `src/capture/` | Move `WinrtCapture`, `DuplicationCapture`. |
| `detection/` | `src/detection/` | Organize backends into subdirs (`directml`, `tensorrt`). Move shaders (`.hlsl`, `.cu`). |
| `input/` | `src/input/` | Move drivers and movement logic. |
| `config/` | `src/config/` | Move `GameProfile` and `AppConfig`. |
| `ui/backend` | `src/ui/backend` | Move D3D11 and ImGui backends. |
| `modules/imgui` | `external/imgui` | Move ImGui source files. |
| `modules/SimpleIni.h` | `external/simpleini/` | Move third-party header. |

**Refactoring Tasks during Migration:**
1.  **Include Paths:** Update `#include "../../core/..."` to relative or absolute project paths (e.g., `#include "core/interfaces/..."`).
2.  **Namespaces:** Ensure everything is consistently wrapped in `namespace sunone` (or rename to `namespace aimbot` if we decide to rebrand, but `sunone` is fine for consistency).
3.  **Shaders:** Ensure `.hlsl` and `.cu` files are copied to the build output directory via CMake.

## 3. Build System (CMake)

The `CMakeLists.txt` will be designed for modularity and modern C++20 standards.

**Key Features:**
- **Target-based architecture:** Each module (core, capture, detection) will be a static library linked into the main executable.
- **Dependency Management:**
    - OpenCV 4.10 (Local or via `find_package`)
    - ONNX Runtime (DirectML)
    - DirectX 11 / DXGI
    - CUDA / TensorRT (Optional, via build flag `USE_CUDA`)
- **Post-Build Steps:** Automatically copy DLLs (OpenCV, ONNXRuntime) and assets to the binary directory.

## 4. Execution Plan

1.  **Scaffold:** Create the directory tree `src/`, `external/`, `cmake/`.
2.  **Dependencies:** Move `modules/*` to `external/` and verify integrity.
3.  **CMake Root:** Create the main `CMakeLists.txt` defining the project and common options.
4.  **Core Module:** Migrate `src/core` and create `src/core/CMakeLists.txt`.
5.  **Sub-Modules:** Iteratively migrate `capture`, `detection`, `input`, `config`, `ui` with their respective CMake files.
6.  **Main:** Create a minimal `src/main.cpp` that instantiates the pipeline to verify linkage.
7.  **Verify:** Run a build `cmake --preset windows-release` and ensure it compiles without errors.

## 5. Definition of Done

- [ ] Project structure created matching the diagram.
- [ ] All code from `extracted_modules` migrated to `src/` or `external/`.
- [ ] Root `CMakeLists.txt` and module `CMakeLists.txt` files created.
- [ ] Project compiles successfully with MSVC (Visual Studio 2022).
- [ ] A "Hello World" pipeline runs (initializes capture and detection classes).
