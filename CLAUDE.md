# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MacroMan AI Aimbot v2 is a modernized C++ implementation of an AI-powered target detection and mouse movement system for first-person shooter games. This project is based on the SunONE C++ aimbot but rebuilt with modern C++ architecture, focusing on modularity, maintainability, and educational value.

**⚠️ IMPORTANT**: This project is strictly educational for learning computer vision, AI inference, and low-level Windows programming. It is NOT intended for competitive or multiplayer game use.

## Core Architecture

The project follows a **modular, interface-driven architecture** with clear separation of concerns:

```
Capture → Detection → Prediction → Mouse Movement
  ↓           ↓          ↓              ↓
Interfaces: IScreenCapture, IDetector, ITargetPredictor, IMouseDriver
```

### Key Modules

**1. Capture Layer** (`extracted_modules/capture/`)
- `DuplicationCapture`: Windows Desktop Duplication API (high performance)
- `WinrtCapture`: Windows.Graphics.Capture API (compatibility fallback)
- Interface: `IScreenCapture`

**2. Detection Layer** (`extracted_modules/detection/`)
- `DMLDetector`: DirectML backend (universal GPU support - NVIDIA/AMD/Intel)
- `TensorRTDetector`: CUDA/TensorRT backend (NVIDIA-only, highest performance)
- `DetectorFactory`: Factory pattern for runtime backend selection
- `PostProcessor`: NMS and bounding box processing
- Interface: `IDetector`

**3. Prediction Layer** (`extracted_modules/input/prediction/`)
- `KalmanPredictor`: Kalman filter for target trajectory prediction
- Interface: `ITargetPredictor`

**4. Input Layer** (`extracted_modules/input/`)
- **Drivers** (`drivers/`):
  - `Win32Driver`: Standard Windows mouse input
  - `ArduinoDriver`: Hardware-based mouse emulation via serial
  - Interface: `IMouseDriver`
- **Movement** (`movement/`):
  - `TrajectoryPlanner`: Bezier curve + OneEuro filter for smooth, human-like movement
  - `BezierCurve`: Parametric curve generation
  - `OneEuroFilter`: Low-latency smoothing filter

**5. Configuration** (`extracted_modules/config/`)
- `AppConfig`: Global application settings
- `GameProfile`: Per-game detection and movement parameters

**6. Core Utilities** (`extracted_modules/core/`)
- **Entities**: `Frame`, `Detection`, `Target`, `MouseMovement`
- **Enums**: Detection mode, mouse driver type, capture method
- **Threading**: `ThreadSafeQueue` for inter-thread communication
- **Utils**: `PathUtils` for cross-platform path handling

## Building the Project

### Prerequisites
- **Windows 10/11** (required for Desktop Duplication API)
- **Visual Studio 2022** with C++ desktop development workload
- **CMake 3.10+** (included with VS2022)
- **CUDA Toolkit 12.x** (optional, for TensorRT backend only)
- **TensorRT 10.x** (optional, included in `modules/TensorRT-10.8.0.43/`)

### Build Commands

#### Using Visual Studio 2022
1. Open project in VS2022: `File → Open → CMake...` → Select `CMakeLists.txt`
2. Select configuration: `x64-Debug` or `x64-Release`
3. Build: `Build → Build All` or `Ctrl+Shift+B`

#### Using CMake Command Line
```bash
# Configure (choose preset)
cmake --preset x64-debug     # For debug build
cmake --preset x64-release   # For release build

# Build
cmake --build out/build/x64-debug
cmake --build out/build/x64-release

# Output location
# Executable: out/build/<preset>/macroman_ai_aimbot.exe
```

### Build Configurations
- **x64-debug**: 64-bit debug with symbols (recommended for development)
- **x64-release**: 64-bit optimized release
- **x86-debug/release**: 32-bit builds (legacy support)

## Dependencies & Third-Party Libraries

All dependencies are **pre-compiled and included** in the repository:

### Core Libraries (`extracted_modules/modules/`)
- **OpenCV 4.10.0** (`opencv/`): Image processing and computer vision
- **TensorRT 10.8.0.43** (`TensorRT-10.8.0.43/`): NVIDIA inference engine (optional)
- **GLFW 3.4** (`glfw-3.4.bin.WIN64/`): Window management and input handling
- **ImGui 1.91.2** (`imgui-1.91.2/`): Immediate-mode GUI overlay
- **serial** (`serial/`): Cross-platform serial port communication (for Arduino driver)
- **SimpleIni.h** (`SimpleIni.h`): INI file parsing (header-only)
- **stb** (`stb/`): Image loading (stb_image.h)

### Tools (`modules/tools/`)
- `sunxds_models.json`: Model configuration metadata
- `base64_encode.py`: Utility for model encoding
- `rgb_565_convert.py`: Image format conversion

## Key Design Patterns

### Factory Pattern
`DetectorFactory` creates detector instances based on backend configuration:
```cpp
auto detector = DetectorFactory::create("dml", config);  // DirectML
auto detector = DetectorFactory::create("tensorrt", config);  // TensorRT
```

### Interface Segregation
Each subsystem exposes a clean interface (`IDetector`, `IMouseDriver`, etc.), allowing:
- Easy mocking for unit tests
- Runtime backend switching
- Minimal coupling between modules

### Configuration Management
Centralized configuration through `AppConfig` with per-game profiles:
- `AppConfig.detector`: Detection settings (thresholds, backend)
- `AppConfig.trajectory`: Mouse movement parameters
- `AppConfig.currentGame`: Active game profile

### Two-Phase Initialization
Detectors support graceful error handling:
```cpp
auto error = detector->loadModel(modelPath);
if (!error.isReady()) {
    // Handle initialization failure with error.errorMessage
}
```

## Development Workflow

### Adding a New Detection Backend
1. Create new class in `extracted_modules/detection/<backend>/`
2. Implement `IDetector` interface
3. Register in `DetectorFactory::create()`
4. Update `AppConfig.detectorBackend` enum

### Adding a New Mouse Driver
1. Create new class in `extracted_modules/input/drivers/`
2. Implement `IMouseDriver` interface
3. Update driver creation logic based on `MouseDriverType` enum

### Adding a New Capture Method
1. Create new class in `extracted_modules/capture/`
2. Implement `IScreenCapture` interface
3. Update capture creation logic based on `AppConfig.captureMethod`

## Important Implementation Details

### Threading Model
- **Capture Thread**: Grabs frames at target FPS
- **Detection Thread**: Runs AI inference asynchronously
- **Movement Thread**: Applies mouse movements with trajectory smoothing
- Communication via `ThreadSafeQueue<T>`

### Coordinate Systems
- **Screen Space**: Absolute pixel coordinates (0,0 = top-left)
- **Detection Space**: Model input resolution (e.g., 640x640 YOLO)
- **Game Space**: Relative to detection region center
- Use `AppConfig.detector.detectionResolution` for scaling

### Performance Considerations
- DirectML backend: ~30-60 FPS on modern integrated GPUs
- TensorRT backend: 100+ FPS on RTX 30-series and newer
- Capture overhead: ~1-2ms with Desktop Duplication
- Target: 144 FPS total pipeline (7ms per frame)

### Model Format
- **DirectML**: ONNX models (`.onnx`)
- **TensorRT**: Engine files (`.engine`) - platform-specific, must be rebuilt per GPU
- Model export tools in `modules/tools/`

## Common Tasks

### Testing a New YOLO Model
1. Place `.onnx` file in `models/` directory
2. Update `modelPath` in configuration
3. Ensure input size matches model (e.g., 640x640)
4. Adjust `numClasses` in `DetectorConfig`

### Debugging Detection Issues
1. Enable `AppConfig.debugMode = true`
2. Enable `AppConfig.detector.verboseLogging = true`
3. Check `DetectorStats` for performance bottlenecks:
   - `preProcessTimeMs`: Image preprocessing
   - `inferenceTimeMs`: Model execution
   - `postProcessTimeMs`: NMS and filtering

### Profiling Mouse Movement
1. Adjust `TrajectoryConfig` parameters:
   - `smoothness`: Higher = slower, smoother movement
   - `minSpeed` / `maxSpeed`: Movement velocity limits
   - `oneEuroFilterBeta`: Responsiveness vs stability
2. Use `BezierCurve` for natural, curved trajectories

## Project Structure Philosophy

This codebase prioritizes:
- **Modularity**: Each component is independent and testable
- **Modern C++**: Leveraging C++17 features (smart pointers, structured bindings, std::optional)
- **Readability**: Clear naming, interfaces over inheritance, minimal macros
- **Extensibility**: Easy to add new backends, drivers, or capture methods

