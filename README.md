# MacroMan AI Aimbot

![CI Status](https://github.com/MacroMan5/marcoman_ai_aimbot/actions/workflows/ci.yml/badge.svg?branch=dev)

This project is an AI-powered aimbot leveraging computer vision.

## Dependencies

The project relies on the following external libraries:

### Included in `external/` folder:
*   **ImGui**: Immediate Mode GUI for C++ with DX11 and Win32 backends.
*   **GLFW**: Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan development.
*   **OpenCV**: Open Source Computer Vision Library (Version 4.10.0 expected in `external/opencv/build`).
*   **SimpleIni**: A cross-platform library that provides a simple API to read and write INI-style configuration files.
*   **Serial**: Cross-platform library for interfacing with RS-232 serial ports.
*   **TensorRT**: NVIDIA TensorRT for high-performance deep learning inference (Optional/Specific configurations).

### Fetched via CMake (FetchContent):
*   **spdlog**: Fast C++ logging library.
*   **Catch2**: A modern, C++-native, header-only, test framework for unit-tests.
*   **Eigen**: C++ template library for linear algebra.

### External Requirements (System/User):
*   **ONNX Runtime DirectML**:
    *   The project currently expects the NuGet package `microsoft.ml.onnxruntime.directml` version `1.22.0` installed at:
        `C:/Users/therouxe/.nuget/packages/microsoft.ml.onnxruntime.directml/1.22.0`
    *   Ensure this package is installed or update `CMakeLists.txt` to point to your installation.
*   **Visual Studio 2022**: With C++ desktop development workload.
*   **Windows SDK**: Version 10.0.26100.0 or later recommended.

## Building the Project

### Using CMake from CLI

1.  Configure the project:
    ```powershell
    cmake -S . -B build
    ```

2.  Build the project:
    ```powershell
    cmake --build build --config Release
    ```

### Using Visual Studio

1.  Open the folder in Visual Studio 2022.
2.  CMake integration should automatically configure the project.
3.  Select your target (e.g., `MacromanAimbot.exe`) and configuration (Release).
4.  Build.

## Troubleshooting

*   **ONNX Runtime Path**: If you encounter errors regarding `onnxruntime`, check that the path in `CMakeLists.txt` matches your system's NuGet package location.
*   **OpenCV**: Ensure `external/opencv/build` contains the valid OpenCV build artifacts.
