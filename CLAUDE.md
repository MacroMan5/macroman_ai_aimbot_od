# CLAUDE.md

**MacroMan AI Aimbot v2** - Modern C++20 real-time computer vision pipeline for educational purposes.

⚠️ **Educational only** - Vision-based CV learning, NOT for competitive use.

---

## 📋 Quick Reference

**Current Status:** @docs/plans/State-Action-Sync/STATUS.md
**Backlog:** @docs/plans/State-Action-Sync/BACKLOG.md

**Detailed guides:**
- @docs/plans/2025-12-29-modern-aimbot-architecture-design.md - Full architecture
- @docs/CRITICAL_TRAPS.md - Implementation failure modes (**READ BEFORE CODING**)

---

## 🔄 State-Action-Sync Workflow

To maintain professional standards while developing with an AI agent, follow this loop:

1.  **State (Sync Context):** Agent reads `docs/plans/State-Action-Sync/STATUS.md` and `docs/plans/State-Action-Sync/BACKLOG.md` to understand the current priority and health.
2.  **Action (Implementation):** Agent implements **one** atomic task from the backlog, including unit tests and verification.
3.  **Sync (Update Docs):** Agent updates `BACKLOG.md` (check off task) and `STATUS.md` (update progress/risks) before concluding.

---

## 🔧 Tech Stack

- **Language:** C++20 (MSVC 2022)
- **Build:** CMake 3.25+
- **Testing:** Catch2 v3 (unit tests), Google Benchmark (performance)
- **Logging:** spdlog
- **Math:** Eigen 3.4 (linear algebra)
- **CV:** OpenCV 4.10 (minimal utils only)
- **AI:** ONNX Runtime 1.19+ (DirectML / TensorRT backends)
- **UI:** Dear ImGui 1.91.2 + GLFW 3.4
- **Platform:** Windows 10/11 (1903+) - Desktop Duplication API required

---

## 📁 Project Structure

```
src/                      # Core logic and Application
├── capture/              # WinrtCapture, DuplicationCapture
├── config/               # AppConfig, GameProfile
├── core/                 # Shared foundation
│   ├── entities/         # Frame, Detection, Target, AimCommand
│   ├── interfaces/       # IScreenCapture, IDetector, IMouseDriver
│   ├── threading/        # LatestFrameQueue, ThreadManager
│   ├── types/            # Common math/geometric types
│   └── utils/            # PathUtils, Logger
├── detection/            # DMLDetector, TensorRTDetector, PostProcessor
├── input/                # Mouse movement & drivers
├── tracking/             # KalmanPredictor
├── ui/                   # Overlay & menus (ImGui)
└── main.cpp              # Application entry point

assets/
└── models/               # .onnx AI models

external/                 # Third-party source/headers
modules/                  # Pre-compiled binaries/SDKs (TensorRT, OpenCV, etc.)
```

---

## 🏗️ Architecture (4-Thread Pipeline)

**Lock-free, zero-copy GPU pipeline:**

```
Thread 1: Capture (PRIORITY_TIME_CRITICAL)
   │ WinrtCapture → GPU Texture
   ▼
LatestFrameQueue (size=1, head-drop)
   │
Thread 2: Detection (PRIORITY_ABOVE_NORMAL)
   │ DMLDetector/TensorRT → YOLO inference → NMS
   ▼
LatestFrameQueue (size=1, head-drop)
   │
Thread 3: Tracking (PRIORITY_NORMAL)
   │ Kalman filter → Target selection → Prediction
   ▼
Atomic AimCommand (single variable)
   │
Thread 4: Input (PRIORITY_HIGHEST, 1000Hz)
   │ Bezier + 1 Euro filter → Mouse movement
```

**Performance targets:** <10ms end-to-end (P95: 15ms), 144+ FPS capture, 60-144 FPS detection

---

## 🚨 Critical Rules (MUST FOLLOW)

### DO:
- ✅ Use `std::unique_ptr`, `std::shared_ptr` (NEVER raw `new`/`delete`)
- ✅ Use lock-free atomics for inter-thread communication
- ✅ Pre-allocate in constructors (NO allocations in hot paths)
- ✅ Read `@docs/CRITICAL_TRAPS.md` before implementing threading/memory code
- ✅ Write Catch2 unit test BEFORE implementation
- ✅ Check `src/core/interfaces/` for existing contracts

### DON'T:
- ❌ `std::mutex` in thread loops (use `std::atomic` or lock-free queues)
- ❌ `std::future::get()` in hot path (blocks thread)
- ❌ `std::vector::push_back()` in frame loops (pre-allocate)
- ❌ Old C++: `NULL`, `typedef`, raw pointers, `printf`
- ❌ Read/write game memory (vision-based only)

---

## 🏃 Build & Run

### Quick Start
```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j

# Run tests
ctest --test-dir build -C Release --output-on-failure

# Output: build/bin/macroman_aimbot.exe
```

### CMake Options
- `-DENABLE_TESTS=ON` - Build unit tests (default: ON)
- `-DENABLE_TENSORRT=ON` - TensorRT backend (requires CUDA 12.x)
- `-DENABLE_BENCHMARKS=OFF` - Benchmark suite (default: OFF)

---

## 🔍 Common Commands

### Testing
```bash
# Run all tests
ctest --test-dir build -C Release

# Run specific test
./build/bin/unit_tests "[queue]"

# Benchmark with golden dataset
./build/bin/sunone-bench --model assets/models/sunxds_0.7.3.onnx --dataset test_data/frames.bin
```

### Development
```bash
# Generate CLAUDE.md (if starting fresh)
claude-code /init

# Format code (if clang-format configured)
cmake --build build --target format

# Analyze code
cmake --build build --target analyze
```

### Code Quality

Before committing code:

1. **Format Code**:
   ```bash
   clang-format -style=file -i src/**/*.cpp src/**/*.h
   ```

2. **Check for Issues**:
   ```bash
   clang-tidy -p build src/your_file.cpp
   ```

CI will automatically check formatting and code quality on all PRs. See `docs/CONTRIBUTING.md` for detailed guidelines.

---

## 🎨 Code Conventions

### Naming
- **Classes/Structs:** PascalCase (`DMLDetector`, `TargetDatabase`)
- **Functions/Methods:** camelCase (`captureFrame()`, `loadModel()`)
- **Variables:** camelCase (`frameCount`, `detectionResults`)
- **Constants:** UPPER_SNAKE_CASE (`MAX_TARGETS`, `DEFAULT_FOV`)
- **Interfaces:** Prefix with `I` (`IDetector`, `IScreenCapture`)

### File Organization
- **Headers:** `src/<module>/<class>.h`
- **Implementation:** `src/<module>/<class>.cpp`
- **One class per file** (except small helper structs)

### Memory Management
- **GPU resources:** Reference-counted via TexturePool + RAII custom deleters
- **CPU memory:** Pre-allocated object pools (NO allocations in hot path)
- **Data structures:** Structure-of-Arrays (SoA) for cache efficiency

### Threading
- **Synchronization:** Lock-free atomics ONLY (no mutexes in loops)
- **Queues:** `LatestFrameQueue<T>` with head-drop policy
- **Ownership:** Each thread owns its data (message passing, no shared state)

---

## 🧪 Testing Requirements

### Unit Tests (Catch2)
```cpp
TEST_CASE("LatestFrameQueue head-drop policy", "[threading]") {
    LatestFrameQueue<TestItem> queue;
    queue.push(new TestItem(1));
    queue.push(new TestItem(2));
    queue.push(new TestItem(3));

    auto* item = queue.pop();
    REQUIRE(item->value == 3);  // Only latest returned
    delete item;
}
```

**Coverage targets:**
- Algorithms: 100% (Kalman, NMS, Bezier)
- Utilities: 80%
- Interfaces: Compile-time only

### Integration Tests
- Use `FakeScreenCapture` with recorded frames
- Golden dataset: `test_data/valorant_500frames.bin`
- Validate end-to-end latency < 10ms

---

## 🛠️ Development Workflow

### Adding a New Component

1. **Define interface** in `extracted_modules/core/interfaces/`
2. **Implement** in `extracted_modules/<module>/<component>/`
3. **Register** in `ComponentFactory`
4. **Write tests** in `tests/unit/`
5. **Verify performance** against targets

**Example:**
```cpp
// 1. Interface
class IDetector {
    virtual std::vector<Detection> detect(Frame* frame) = 0;
};

// 2. Implementation
class MyDetector : public IDetector { /* ... */ };

// 3. Register
factory.registerImplementation<IDetector>("MyBackend",
    []() { return std::make_unique<MyDetector>(); });

// 4. Test
TEST_CASE("MyDetector loads model") { /* ... */ }
```

---

## 📊 Performance Budgets

| Stage | Target | P95 | Failure Indicator |
|-------|--------|-----|------------------|
| Capture | <1ms | 2ms | GPU busy, driver lag |
| Inference | 5-8ms | 10ms | Model too large |
| NMS | <1ms | 2ms | >100 detections |
| Tracking | <1ms | 2ms | >64 targets |
| Input | <0.5ms | 1ms | Filter complexity |
| **TOTAL** | **<10ms** | **15ms** | Any stage over target |

**Resource limits:**
- VRAM: <512MB (model + buffers)
- RAM: <1GB (working set)
- CPU: <15% per thread (8-core CPU)

---

## 🔗 Key Patterns

### Factory Pattern
```cpp
// Runtime backend selection
auto detector = factory.create<IDetector>(config.backend);
auto driver = factory.create<IMouseDriver>(config.driver);
```

### Data-Oriented Design (SoA)
```cpp
// Cache-efficient parallel arrays
struct TargetDatabase {
    std::array<Vec2, 64> positions;
    std::array<Vec2, 64> velocities;
    std::array<float, 64> confidences;
    // Iterate positions only → 2-3x faster
};
```

### Two-Phase Initialization
```cpp
auto detector = factory.create<IDetector>("DirectML");
if (!detector->loadModel(path)) {
    LOG_ERROR("Failed: {}", detector->getLastError());
}
```

---

## 📖 Additional Resources

- [Anthropic Claude Code Best Practices](https://www.anthropic.com/engineering/claude-code-best-practices)
- [Real-Time CV Inference Latency](https://blog.roboflow.com/inference-latency/)
- [Lock-Free Queue Performance](https://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++)
- [1 Euro Filter (smoothing)](https://cristal.univ-lille.fr/~casiez/1euro/)
- [Kalman Filter Tutorial](https://www.cs.unc.edu/~welch/kalman/)

---

**Current Phase:** Phase 1 - Foundation (see @docs/plans/2025-12-29-phase1-foundation.md)
**Status:** ✅ CMake configured, interfaces defined, LatestFrameQueue implemented
**Next:** Thread manager, logging system, unit tests

---

*Last updated: 2025-12-30 | Version: 2.1 (Concise)*
