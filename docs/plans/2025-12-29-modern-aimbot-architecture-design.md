# Modern AI Aimbot - Architecture Design Document

**Project:** Marcoman AI Aimbot (Modern C++ Rewrite)
**Date:** 2025-12-29
**Version:** 1.0 (MVP Scope)
**Author:** Design Session with Claude Code

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Design Goals & Requirements](#design-goals--requirements)
3. [System Architecture](#system-architecture)
4. [Threading & Concurrency Model](#threading--concurrency-model)
5. [Data Flow & Memory Management](#data-flow--memory-management)
6. [Module Specifications](#module-specifications)
7. [Configuration & Plugin System](#configuration--plugin-system)
8. [Testing & Observability](#testing--observability)
9. [Performance Targets](#performance-targets)
10. [MVP Scope & Future Expansion](#mvp-scope--future-expansion)
11. [Implementation Roadmap](#implementation-roadmap)
12. [Appendix](#appendix)

---

## Executive Summary

This document defines the architecture for a **modern, high-performance AI-powered aimbot** written in C++. The system uses real-time computer vision (YOLO object detection) to identify targets in FPS games and provides smooth, human-like mouse control through advanced trajectory planning.

**Key Design Principles:**
- **Low Latency**: <10ms end-to-end (capture → detection → input)
- **GPU Efficient**: Shares GPU with game (zero-copy paths, <512MB VRAM budget)
- **Modern C++**: Interface-based design, data-oriented structures, lock-free concurrency
- **Observable**: Runtime metrics, visual debugging, component toggles
- **Extensible**: Plugin architecture for swappable implementations

**Target Hardware:**
- Primary: NVIDIA RTX 3070 Ti (representative modern gaming GPU)
- Support: Any DirectML-capable GPU (AMD, Intel, NVIDIA)

**Technology Stack:**
- **Language**: C++20
- **Build System**: CMake (cross-platform, Windows primary)
- **CV Framework**: OpenCV 4.10
- **AI Inference**: ONNX Runtime (DirectML + TensorRT backends)
- **UI**: Dear ImGui (overlay + external config app)
- **Threading**: Lock-free queues (moodycamel::ConcurrentQueue)
- **Testing**: Catch2 (unit tests), Google Benchmark (performance)

---

## Design Goals & Requirements

### Functional Requirements (MVP)

| Feature | Description | Priority |
|---------|-------------|----------|
| **Screen Capture** | 144+ FPS capture via WinRT/Desktop Duplication | P0 |
| **AI Detection** | YOLO inference at 60-144 FPS | P0 |
| **Target Tracking** | Multi-target tracking with Kalman filtering | P0 |
| **Smooth Aiming** | Bezier curves + 1 Euro filter | P0 |
| **Auto Game Detection** | Detect active game and load profile | P0 |
| **Profile Management** | Per-game settings (FOV, smoothness, model) | P0 |
| **Live Config Tuning** | Real-time parameter adjustment via UI | P0 |
| **Debug Overlay** | In-game metrics and bounding boxes | P1 |
| **External Config UI** | Standalone app for profile editing | P1 |

### Non-Functional Requirements

| Requirement | Target | Measurement |
|-------------|--------|-------------|
| **End-to-End Latency** | <10ms (P95) | Capture timestamp → mouse movement |
| **Frame Processing** | 144 FPS @ 640x640 input | Frames/second sustained |
| **VRAM Usage** | <512MB | DirectX resource tracking |
| **CPU Overhead** | <15% on 8-core CPU | Per-thread profiling |
| **Startup Time** | <3 seconds | Cold start to ready |
| **Memory Footprint** | <1GB RAM | Process working set |

### Out of Scope (Post-MVP)

- ❌ Automatic weapon detection (interfaces defined, not implemented)
- ❌ Recoil compensation system (architecture ready, not implemented)
- ❌ Multi-model preloading (single model only in MVP)
- ❌ Cloud-based profile sharing
- ❌ Anti-cheat evasion techniques

---

## System Architecture

### High-Level Overview

The system uses a **layered hybrid architecture** combining pipeline parallelism for real-time processing with data-oriented storage for game entities.

```
┌─────────────────────────────────────────────────────────────┐
│              External Configuration UI (ImGui)               │
│                    (Standalone Process)                      │
└────────────────────┬────────────────────────────────────────┘
                     │ IPC (Shared Memory + Named Pipes)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                   Core Engine Process                        │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │             Pipeline Layer (4 Threads)               │   │
│  │                                                      │   │
│  │  [Capture] → [Detection] → [Tracking] → [Input]    │   │
│  │      ↓            ↓            ↓          ↓         │   │
│  │      └── Lock-Free Queues (Size-1, Latest) ────────┘   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          Entity Layer (SoA TargetDatabase)           │   │
│  │  - Positions, Velocities, Confidences, Predictions  │   │
│  │  - SIMD-friendly parallel arrays                     │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │               System Layer (Services)                │   │
│  │  - ModelManager, ProfileManager, GameDetector        │   │
│  │  - Dependency Injection, Plugin Registry             │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         In-Game Overlay (ImGui Transparent)          │   │
│  │  - Debug metrics, bounding boxes, component toggles  │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Physical Architecture

**Processes:**
1. **Core Engine** (`marcoman_aimbot.exe`)
   - Main process running pipeline threads
   - Overlay rendering
   - GPU inference

2. **Configuration UI** (`marcoman_config.exe`)
   - Profile editor
   - Metrics dashboard
   - Component swapping

**Inter-Process Communication:**
- **Shared Memory**: Hot-path config (smoothness, FOV, enable flags)
- **Named Pipes**: Cold-path commands (load profile, switch model, shutdown)

---

## Threading & Concurrency Model

### Pipeline Architecture

Based on research ([Roboflow Inference Pipeline](https://blog.roboflow.com/inference-latency/), [YOLO Thread-Safe Inference](https://docs.ultralytics.com/guides/yolo-thread-safe-inference/)), we use **dedicated threads per stage** with **lock-free queues** for minimal latency.

```
Thread 1: Capture (Priority: Highest)
    │
    │ LatestFrameQueue (size=1, overwrites old)
    ▼
Thread 2: Detection (GPU-bound)
    │
    │ LatestFrameQueue (size=1, overwrites old)
    ▼
Thread 3: Tracking (CPU-bound, owns TargetDatabase)
    │
    │ Atomic write to shared AimCommand
    ▼
Thread 4: Input (1000Hz loop, reads AimCommand)
```

**Thread Priorities:**
- Capture: `THREAD_PRIORITY_TIME_CRITICAL`
- Detection: `THREAD_PRIORITY_ABOVE_NORMAL`
- Tracking: `THREAD_PRIORITY_NORMAL`
- Input: `THREAD_PRIORITY_HIGHEST`

### Backpressure Strategy (Head-Drop Policy)

**Problem:** If detection is slower than capture (e.g., 60 FPS vs 144 FPS), processing old frames leads to aiming at stale positions.

**Solution:** Always process the **latest available frame**. Drop intermediate frames.

```cpp
template<typename T>
class LatestFrameQueue {
    std::atomic<T*> slot{nullptr};

public:
    void push(T* new_frame) {
        T* old = slot.exchange(new_frame, std::memory_order_release);
        if (old) delete old;  // Drop old frame
    }

    T* pop() {
        return slot.exchange(nullptr, std::memory_order_acquire);
    }
};
```

**Rationale:** Real-time systems prioritize **freshness over completeness**. It's better to skip 5 frames and process the latest than to be 100ms behind reality.

### Synchronization Mechanisms

| Component | Mechanism | Justification |
|-----------|-----------|---------------|
| **Frame Queues** | Lock-free atomic exchange | <1μs latency, no contention |
| **TargetDatabase** | Message passing (owned by Tracking) | No shared state between Detection/Tracking |
| **AimCommand** | Single atomic variable | Input thread reads, Tracking writes |
| **Config** | std::atomic fields in shared memory | Lock-free hot-path tuning |
| **Texture Pool** | Atomic ref-counting | Safe GPU resource sharing |

**No Mutexes in Hot Path.** All critical-path synchronization is lock-free.

---

## Data Flow & Memory Management

### Frame-by-Frame Flow (Annotated Timeline)

```
T+0ms:  [Capture Thread]
        - WinrtCapture::captureFrame() → D3D11Texture2D
        - Create Frame{texture, timestamp=now(), seqId=N}
        - Acquire from TexturePool (refCount++)
        - Push to DetectionQueue (overwrites Frame N-1 if detector busy)

T+2ms:  [Detection Thread]
        - Pop latest Frame from queue
        - TexturePool: refCount++ (now owned by detector)
        - DirectML/TensorRT inference (GPU async via CUDA stream)
        - NMS post-processing (CPU)
        - Create DetectionBatch{observations, timestamp}
        - Release texture: refCount--
        - Push DetectionBatch to TrackingQueue

T+6ms:  [Tracking Thread]
        - Pop DetectionBatch
        - Data association: match observations to existing tracks
        - Update Kalman filters for all targets
        - Predict positions at (now + inputLatency + processingDelay)
        - Select best target (closest to crosshair, hitbox priority)
        - Snapshot config: smoothness, FOV, profileId
        - Atomic store: latestCommand = AimCommand{...}

T+0-∞:  [Input Thread @ 1000Hz]
        - Every 1ms:
            - Atomic load: cmd = latestCommand
            - Extrapolate target position to current time
            - TrajectoryPlanner: Bezier curve step + 1 Euro filter
            - MouseDriver: send movement (async for Arduino, sync for Win32)
```

**Total Latency Budget:** ~6-10ms (capture → first mouse movement)

### Memory Management Strategy

#### 1. GPU Memory (Zero-Copy Path)

**Texture Pool (Triple Buffer):**
```cpp
class TexturePool {
    struct Texture {
        ID3D11Texture2D* d3dTexture;
        std::atomic<int> refCount{0};
        uint64_t frameId;
    };

    std::array<Texture, 3> pool;  // Triple buffer

public:
    Texture* acquireForWrite() {
        // Find texture with refCount == 0
        // If all busy (detector is slow), return nullptr → drop frame
    }

    void release(Texture* tex) {
        tex->refCount.fetch_sub(1);
    }
};
```

**VRAM Budget Tracking:**
```cpp
class GPUMemoryManager {
    size_t maxVRAM = 512_MB;  // Reserve for aimbot
    std::atomic<size_t> currentUsage{0};

    // Pre-allocated inference buffers (reused every frame)
    ID3D11Buffer* inputTensor;   // 640x640x3 FP16
    ID3D11Buffer* outputTensor;  // 8400x85
};
```

#### 2. CPU Memory (Object Pools)

**Frame Allocator (Avoid Per-Frame Malloc):**
```cpp
class FrameAllocator {
    ObjectPool<Frame> framePool{10};
    ObjectPool<DetectionBatch> batchPool{5};
    ObjectPool<std::vector<Detection>> vectorPool{5};

    // Objects recycled after use, no allocations in hot path
};
```

#### 3. TargetDatabase (Structure-of-Arrays)

**Owned by Tracking Thread (No Shared State):**
```cpp
struct TargetDatabase {
    static constexpr size_t MAX_TARGETS = 64;

    // Pre-allocated parallel arrays (SoA for cache efficiency)
    std::array<TargetID, MAX_TARGETS> ids;
    std::array<BBox, MAX_TARGETS> bboxes;
    std::array<Vec2, MAX_TARGETS> velocities;
    std::array<Vec2, MAX_TARGETS> positions;
    std::array<float, MAX_TARGETS> confidences;
    std::array<HitboxType, MAX_TARGETS> hitboxTypes;
    std::array<time_point, MAX_TARGETS> lastSeen;
    std::array<KalmanState, MAX_TARGETS> kalmanStates;

    size_t count{0};  // Active targets

    // SIMD-friendly iteration (process 4 targets at once)
    void updatePredictions(float dt) {
        for (size_t i = 0; i < count; i += 4) {
            __m256 vel_x = _mm256_load_ps(&velocities[i].x);
            __m256 pos_x = _mm256_load_ps(&positions[i].x);
            __m256 dt_vec = _mm256_set1_ps(dt);
            __m256 new_pos_x = _mm256_fmadd_ps(vel_x, dt_vec, pos_x);
            _mm256_store_ps(&positions[i].x, new_pos_x);
        }
    }
};
```

**Rationale for SoA:**
- **Cache Efficiency**: When iterating over positions to find closest target, only position data is loaded into cache (not entire Target structs)
- **SIMD Potential**: AVX2 can process 4-8 floats in parallel
- **Hot/Cold Split**: Frequently accessed data (positions, velocities) separate from cold data (debug names, timestamps)

#### 4. Shared Configuration (IPC)

**Memory-Mapped File:**
```cpp
struct SharedConfig {
    // Hot-path tunables (atomic for lock-free access)
    std::atomic<float> aimSmoothness{0.5f};
    std::atomic<float> fov{60.0f};
    std::atomic<uint32_t> activeProfileId{0};
    std::atomic<bool> enablePrediction{true};
    std::atomic<bool> enableAiming{true};

    char padding1[64];  // Avoid false sharing

    // Telemetry (written by engine, read by UI)
    std::atomic<float> currentFPS{0.0f};
    std::atomic<float> detectionLatency{0.0f};
    std::atomic<int> activeTargets{0};
    std::atomic<size_t> vramUsageMB{0};
};
```

**Access Pattern:**
- **Core Engine**: Reads config every frame (atomic load, ~1 cycle)
- **Config UI**: Writes when user drags slider (atomic store)
- **No locks, no syscalls** in the hot path

### Unified Clock & Timestamping

**Problem:** Without synchronized timestamps, prediction is inaccurate. A frame captured at T=100ms might be processed at T=110ms. If we don't account for this 10ms, we aim at where the target *was*, not where it *is*.

**Solution:** Every `Frame` carries its capture timestamp. All predictions extrapolate to `now() + latency`.

```cpp
struct Frame {
    ID3D11Texture2D* texture;
    std::chrono::high_resolution_clock::time_point captureTime;
    uint64_t frameSequence;  // For debugging dropped frames
};

struct Target {
    Vec2 position;
    Vec2 velocity;
    std::chrono::high_resolution_clock::time_point observedAt;  // From Frame::captureTime
};

// In Tracking Thread:
auto now = high_resolution_clock::now();
auto latency = estimatedProcessingTime + estimatedInputLag;  // Calibrated per game
Vec2 predictedPos = kalman.predict(target, now + latency - target.observedAt);
```

**Latency Calibration:**
- Measure round-trip: capture → detection → input → screen update
- Store per-game profiles (some games have more input lag)
- Runtime adjustment via config UI

---

## Module Specifications

### 1. Capture Module

**Interface:**
```cpp
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;
    virtual Frame* captureFrame() = 0;
    virtual void initialize(HWND targetWindow) = 0;
    virtual void shutdown() = 0;
};
```

**Implementations:**

| Class | Backend | Performance | Compatibility |
|-------|---------|-------------|---------------|
| `WinrtCapture` | Windows.Graphics.Capture | 144+ FPS | Windows 10 1903+ |
| `DuplicationCapture` | Desktop Duplication API | 120+ FPS | Windows 8+ |

**WinrtCapture (Preferred):**
- Captures specific window (not full desktop)
- Hardware-accelerated on GPU
- Lower CPU overhead than Desktop Duplication

**Location:** `extracted_modules/capture/`

---

### 2. Detection Module

**Interface:**
```cpp
class IDetector {
public:
    virtual ~IDetector() = default;
    virtual void loadModel(const std::string& modelPath) = 0;
    virtual std::vector<Detection> detect(Frame* frame) = 0;
    virtual void shutdown() = 0;
};

struct Detection {
    BBox bbox;           // {x, y, width, height}
    float confidence;
    int classId;
    HitboxType hitbox;   // Head, chest, body (from class mapping)
};
```

**Implementations:**

| Class | Backend | Target GPU | Latency (640x640) |
|-------|---------|------------|-------------------|
| `DMLDetector` | DirectML (ONNX Runtime) | All GPUs | 8-12ms |
| `TensorRTDetector` | NVIDIA TensorRT | NVIDIA only | 5-8ms |

**Post-Processing:**
```cpp
class PostProcessor {
public:
    static std::vector<Detection> applyNMS(
        const std::vector<Detection>& detections,
        float iouThreshold
    );

    static void filterByConfidence(
        std::vector<Detection>& detections,
        float minConfidence
    );
};
```

**Location:** `extracted_modules/detection/`

---

### 3. Tracking Module

**Interface:**
```cpp
class ITargetTracker {
public:
    virtual ~ITargetTracker() = default;
    virtual void update(const DetectionBatch& batch) = 0;
    virtual std::optional<Target> selectBestTarget(const Vec2& crosshairPos, float fov) = 0;
};

class IPredictor {
public:
    virtual ~IPredictor() = default;
    virtual void update(const Vec2& observation, float dt) = 0;
    virtual Vec2 predict(float dt) = 0;
};
```

**Implementation (Kalman Filter):**
```cpp
class KalmanPredictor : public IPredictor {
    Eigen::Vector4f state;  // [x, y, vx, vy]
    Eigen::Matrix4f P;      // Covariance
    Eigen::Matrix4f Q;      // Process noise
    Eigen::Matrix2f R;      // Measurement noise

public:
    void update(const Vec2& observation, float dt) override;
    Vec2 predict(float dt) override;
};
```

**Target Selection Strategy:**
1. Filter by FOV (circular region around crosshair)
2. Sort by distance to crosshair
3. Prioritize by hitbox (head > chest > body)
4. Return closest valid target

**Location:** `extracted_modules/input/prediction/`, `extracted_modules/core/`

---

### 4. Input Module

**Interface:**
```cpp
class IMouseDriver {
public:
    virtual ~IMouseDriver() = default;
    virtual void move(int dx, int dy) = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;  // Block until pending ops complete
};
```

**Implementations:**

| Class | Method | Latency | Detection Risk |
|-------|--------|---------|----------------|
| `Win32Driver` | SendInput API | <1ms | Detectable |
| `ArduinoDriver` | Serial HID emulation | 2-5ms | Hardware-level |

**Trajectory Planning:**
```cpp
class TrajectoryPlanner {
    BezierCurve curve;
    OneEuroFilter filter;

public:
    struct Config {
        bool bezierEnabled{true};
        float smoothness{0.5f};       // 0=instant, 1=very smooth
        float filterMinCutoff{1.0f};  // 1 Euro filter params
        float filterBeta{0.007f};
    };

    Vec2 step(const AimCommand& cmd, const Vec2& predictedTarget);
};
```

**Bezier Curve Smoothing:**
- Cubic Bezier from current position to target
- Control points based on smoothness parameter
- Evaluated incrementally over 1000Hz loop

**1 Euro Filter:**
- Adaptive low-pass filter
- Reduces jitter during slow movements
- Maintains responsiveness during flicks

**Location:** `extracted_modules/input/`

---

### 5. Configuration Module

**Profile Structure:**
```json
{
  "gameId": "valorant",
  "displayName": "Valorant",
  "processNames": ["VALORANT.exe"],
  "windowTitles": ["VALORANT"],

  "detection": {
    "modelPath": "models/valorant_yolov8_640.onnx",
    "inputSize": [640, 640],
    "confidenceThreshold": 0.6,
    "nmsThreshold": 0.45,
    "hitboxMapping": {
      "0": "head",
      "1": "chest",
      "2": "body"
    }
  },

  "targeting": {
    "fov": 80.0,
    "smoothness": 0.65,
    "predictionStrength": 0.8,
    "hitboxPriority": ["head", "chest", "body"],
    "inputLatency": 12.5
  }
}
```

**Auto-Game Detection:**
```cpp
class GameDetector {
    std::vector<GameProfile> profileRegistry;
    std::optional<GameInfo> candidateGame;
    std::chrono::time_point candidateTime;

public:
    void poll() {  // Called every 500ms on background thread
        HWND hwnd = GetForegroundWindow();
        std::string procName = getProcessName(hwnd);

        for (auto& profile : profileRegistry) {
            if (profile.matches(procName)) {
                if (candidateGame != profile.gameId) {
                    // New game detected, start hysteresis timer
                    candidateGame = profile.gameId;
                    candidateTime = now();
                }
                else if (now() - candidateTime > 3s) {
                    // Stable for 3 seconds, commit switch
                    onGameChanged(profile);
                    candidateGame.reset();
                }
                return;
            }
        }
    }
};
```

**Hysteresis Rationale:** Prevents rapid model swaps when alt-tabbing to Discord/browser and back.

**Location:** `extracted_modules/config/`

---

### 6. Plugin System

**Factory Pattern for Runtime Swapping:**
```cpp
class ComponentFactory {
    template<typename Interface>
    using Creator = std::function<std::unique_ptr<Interface>()>;

    template<typename Interface>
    void registerImplementation(const std::string& name, Creator<Interface> creator);

    template<typename Interface>
    std::unique_ptr<Interface> create(const std::string& name);
};

// Registration (at startup):
factory.registerImplementation<IScreenCapture>("WinrtCapture",
    []() { return std::make_unique<WinrtCapture>(); });

factory.registerImplementation<IDetector>("DirectML",
    []() { return std::make_unique<DMLDetector>(); });

// Usage (from config file):
auto capture = factory.create<IScreenCapture>(config.captureImpl);
```

**Plugin Configuration (`config/plugins.ini`):**
```ini
[Capture]
Implementation=WinrtCapture

[Detection]
Backend=DirectML
Device=GPU

[Input]
Driver=Win32Driver

[Prediction]
Method=KalmanFilter
```

**Runtime Swapping (Thread-Safe):**
```cpp
void switchInputDriver(const std::string& newDriver) {
    inputThread.pauseAndDrain();  // Wait for pending operations

    mouseDriver->shutdown();  // Blocking cleanup
    mouseDriver.reset();

    mouseDriver = factory.create<IMouseDriver>(newDriver);
    mouseDriver->initialize();

    inputThread.resume();
}
```

---

## Configuration & Plugin System

### Configuration Hierarchy (3 Levels)

**1. Global Config (`config/global.ini`)**
```ini
[Engine]
MaxFPS=144
VRAMBudget=512
LogLevel=Info
ThreadAffinity=true  # Pin threads to specific cores

[UI]
OverlayHotkey=HOME
ShowDebugInfo=true
Theme=Dark

[IPC]
SharedMemoryName=MarcomanAimbot_Config
CommandPipeName=MarcomanAimbot_Commands
```

**2. Game Profiles (`config/games/{game_name}.json`)**
See Module Specifications section for full schema.

**3. Runtime Config (Shared Memory)**
See Data Flow section for `SharedConfig` structure.

### Model Management (MVP - Single Model)

```cpp
class ModelManager {
    std::unique_ptr<ONNXModel> currentModel;
    ID3D11Device* d3dDevice;  // Shared for zero-copy

public:
    void switchModel(const std::string& modelPath) {
        // 1. Pause detection thread
        detectionThread.pauseAndDrain();

        // 2. Unload old model (free VRAM)
        currentModel.reset();

        // 3. Load new model
        currentModel = std::make_unique<ONNXModel>(modelPath, d3dDevice);

        // 4. Resume detection
        detectionThread.resume();

        LOG_INFO("Model switched: {} ({} MB VRAM)",
                 modelPath, getCurrentVRAM());
    }
};
```

**VRAM Enforcement:**
- Single model loaded at any time (no preloading in MVP)
- Explicit cleanup before loading new model
- Budget tracking via DirectX resource queries

---

## Testing & Observability

### Testing Strategy

**1. Algorithm Unit Tests (Catch2)**

Test pure functions in isolation:
```cpp
TEST_CASE("Kalman Filter Prediction", "[prediction]") {
    KalmanPredictor kalman;

    kalman.update({100, 100}, 0.016);  // Frame 1: target at (100,100)
    kalman.update({110, 100}, 0.016);  // Frame 2: moved 10px right

    auto predicted = kalman.predict(0.048);  // Predict 3 frames ahead

    REQUIRE(predicted.x == Approx(130.0).epsilon(0.1));
    REQUIRE(predicted.y == Approx(100.0).epsilon(0.1));
}

TEST_CASE("NMS Post-Processing", "[detection]") {
    std::vector<Detection> dets = {
        {BBox{10, 10, 50, 50}, 0.9f, 0},
        {BBox{15, 15, 55, 55}, 0.8f, 0},  // Overlapping (IoU > 0.5)
        {BBox{200, 200, 250, 250}, 0.85f, 0}
    };

    auto filtered = PostProcessor::applyNMS(dets, 0.5);

    REQUIRE(filtered.size() == 2);  // One overlap removed
    REQUIRE(filtered[0].confidence == 0.9f);  // Kept highest
}
```

**2. Integration Tests (Fake Implementations)**

Test full pipeline with recorded data:
```cpp
class FakeScreenCapture : public IScreenCapture {
    std::vector<Frame> recordedFrames;
    size_t index{0};

public:
    void loadRecording(const std::string& path) {
        recordedFrames = loadFramesFromDisk(path);
    }

    Frame* captureFrame() override {
        return &recordedFrames[index++ % recordedFrames.size()];
    }
};

TEST_CASE("Full Pipeline with Golden Dataset", "[integration]") {
    auto capture = std::make_unique<FakeScreenCapture>();
    capture->loadRecording("test_data/valorant_500frames.bin");

    auto detector = std::make_unique<FakeDetector>();
    detector->setResults({Detection{BBox{320, 240, 50, 80}, 0.9f, 0}});

    Pipeline pipeline(std::move(capture), std::move(detector));
    pipeline.run(500);

    REQUIRE(pipeline.getAverageLatency() < 10.0);
    REQUIRE(pipeline.getDetectionCount() == 500);
}
```

**3. CLI Benchmark Tool (`sunone-bench.exe`)**

Headless performance regression testing for CI/CD:
```bash
sunone-bench.exe \
  --dataset test_data/valorant_golden.frames \
  --model models/valorant_yolov8.onnx \
  --threshold-avg-fps 120 \
  --threshold-p99-latency 15.0

# Output:
# ========================================
# Benchmark Results
# ========================================
# Frames Processed:    500
# Average FPS:         138.7  [✓ PASS: >= 120]
# P95 Latency:         8.3ms
# P99 Latency:         12.1ms [✓ PASS: <= 15.0]
# Detected Targets:    487
# ========================================
# VERDICT: PASS
```

**CI/CD Integration:**
```yaml
# .github/workflows/performance-regression.yml
- name: Run Performance Benchmark
  run: |
    sunone-bench.exe \
      --dataset test_data/valorant_500frames.bin \
      --model models/valorant_yolov8.onnx \
      --threshold-avg-fps 120 \
      --threshold-p99-latency 15.0
    # Fails build if thresholds not met
```

### Runtime Observability

**1. Telemetry System (Lock-Free Metrics)**

```cpp
struct Metrics {
    struct ThreadMetrics {
        std::atomic<uint64_t> frameCount{0};
        std::atomic<float> avgLatency{0.0f};  // EMA
        std::atomic<float> maxLatency{0.0f};
        std::atomic<uint64_t> droppedFrames{0};
    };

    ThreadMetrics capture;
    ThreadMetrics detection;
    ThreadMetrics tracking;
    ThreadMetrics input;

    std::atomic<int> activeTargets{0};
    std::atomic<float> fps{0.0f};
    std::atomic<size_t> vramUsageMB{0};
};

// Updated in each thread (no locks):
void captureThreadLoop() {
    while (running) {
        Timer timer;
        auto frame = captureFrame();

        float latency = timer.elapsed();
        metrics.capture.frameCount++;
        metrics.capture.avgLatency =
            metrics.capture.avgLatency * 0.95f + latency * 0.05f;  // EMA
    }
}
```

**2. Visual Debug Overlay (ImGui)**

```cpp
void DebugOverlay::render() {
    ImGui::Begin("Debug");

    // Performance metrics
    auto tel = telemetryExport.load();
    ImGui::Text("FPS: %.1f", tel.captureFPS);
    ImGui::Text("Latency: %.2f ms", tel.endToEndLatency);
    ImGui::ProgressBar(tel.vramUsageMB / 512.0f, ImVec2(-1, 0),
                      std::format("{} MB / 512 MB", tel.vramUsageMB).c_str());

    // Live bounding boxes
    if (showBBoxes) {
        auto drawList = ImGui::GetBackgroundDrawList();
        auto db = targetDatabase.snapshot();

        for (size_t i = 0; i < db.count; ++i) {
            ImU32 color = (i == selectedTarget) ? GREEN : RED;
            drawList->AddRect(
                ImVec2(db.bboxes[i].x, db.bboxes[i].y),
                ImVec2(db.bboxes[i].x + db.bboxes[i].w,
                       db.bboxes[i].y + db.bboxes[i].h),
                color, 0.0f, 0, 2.0f
            );
        }
    }

    // Component toggles
    ImGui::Checkbox("Enable Tracking", &config.enableTracking);
    ImGui::Checkbox("Enable Prediction", &config.enablePrediction);

    if (ImGui::Button("Reload Model")) {
        modelManager->reloadCurrentModel();
    }

    ImGui::End();
}
```

**3. Frame Time Profiler**

```cpp
class FrameProfiler {
    static constexpr size_t HISTORY_SIZE = 300;  // 5 sec @ 60fps

    std::array<float, HISTORY_SIZE> captureHistory;
    std::array<float, HISTORY_SIZE> detectionHistory;
    size_t writeIndex{0};

public:
    void renderGraph() {
        ImGui::PlotLines("Capture (ms)", captureHistory.data(), HISTORY_SIZE);
        ImGui::PlotLines("Detection (ms)", detectionHistory.data(), HISTORY_SIZE);
    }

    void detectBottleneck() {
        float avg = std::accumulate(detectionHistory.begin(),
                                   detectionHistory.end(), 0.0f) / HISTORY_SIZE;

        if (avg > 15.0f) {
            ImGui::TextColored(RED, "Bottleneck: Detection (%.2f ms)", avg);
            ImGui::TextWrapped("Suggestion: Reduce input size or switch to TensorRT");
        }
    }
};
```

**4. Logging (spdlog)**

```cpp
// Structured logging with rotating files
Logger::init();

LOG_INFO("Engine started");
LOG_INFO("Model loaded: {} ({} MB VRAM)", modelPath, vramUsage);
LOG_WARN("Detection slow: {:.2f}ms (target: <10ms)", latency);
LOG_ERROR("Failed to initialize capture: {}", error);
```

**Log Levels:**
- `TRACE`: Frame-by-frame details (disabled in release)
- `DEBUG`: Component lifecycle (init, shutdown)
- `INFO`: Normal operation (model loaded, game switched)
- `WARN`: Performance degradation
- `ERROR`: Recoverable failures
- `CRITICAL`: Unrecoverable errors (shutdown)

---

## Performance Targets

### Latency Budget (End-to-End: <10ms)

| Stage | Target | P95 | Measurement |
|-------|--------|-----|-------------|
| **Screen Capture** | <1ms | 2ms | WinRT present → texture ready |
| **GPU Inference** | 5-8ms | 10ms | Texture → detection results |
| **Post-Processing** | <1ms | 2ms | NMS + filtering |
| **Tracking Update** | <1ms | 2ms | Kalman filters + selection |
| **Input Planning** | <0.5ms | 1ms | Bezier + 1 Euro filter |
| **Total** | **<10ms** | **15ms** | Capture timestamp → mouse move |

### Throughput Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Capture FPS** | 144+ | Frames/second sustained |
| **Detection FPS** | 60-144 | Model-dependent |
| **Input Rate** | 1000Hz | Mouse updates/second |

### Resource Budgets

| Resource | Budget | Enforcement |
|----------|--------|-------------|
| **VRAM** | <512MB | Model + buffers only |
| **RAM** | <1GB | Working set |
| **CPU (per-thread)** | <15% | On 8-core CPU |
| **GPU Compute** | <20% | Shared with game |

**GPU Sharing Strategy:**
- Use CUDA streams for async execution
- Lower thread priority when game is active
- Monitor GPU usage via NVML/DirectX queries

---

## MVP Scope & Future Expansion

### MVP Features (In Scope)

✅ **Core Pipeline:**
- Screen capture (WinRT + Desktop Duplication)
- YOLO detection (DirectML + TensorRT)
- Multi-target tracking (Kalman filter)
- Smooth aiming (Bezier + 1 Euro filter)

✅ **Configuration:**
- Auto game detection (with hysteresis)
- Per-game profiles (FOV, smoothness, model)
- Live config tuning (shared memory IPC)

✅ **Input:**
- Win32Driver (SendInput)
- ArduinoDriver (serial HID emulation)

✅ **Observability:**
- Debug overlay (metrics, bounding boxes)
- External config UI (ImGui standalone)
- CLI benchmark tool

### Post-MVP Features (Architected, Not Implemented)

🔲 **Auto Weapon Detection:**
- Interface: `IWeaponDetector`
- MVP Implementation: `NullWeaponDetector` (always returns `nullopt`)
- Future: Visual detection of equipped weapon (bottom of screen, high confidence)

🔲 **Recoil Compensation:**
- Interface: `IRecoilCompensator`
- MVP Implementation: `NullRecoilCompensator` (pass-through)
- Future: CSV-based recoil patterns, triggered by mouse hook (not visual detection)

🔲 **Advanced Features:**
- Multi-model preloading (with VRAM budget management)
- Cloud profile sharing
- Machine learning for auto-calibration
- Anti-anti-cheat techniques (out of scope for legal/ethical reasons)

### Interface Stubs for Future Expansion

```cpp
// Weapon detection (not implemented in MVP)
class IWeaponDetector {
public:
    virtual ~IWeaponDetector() = default;
    virtual std::optional<WeaponInfo> detectWeapon(const std::vector<Detection>&) = 0;
};

class NullWeaponDetector : public IWeaponDetector {
    std::optional<WeaponInfo> detectWeapon(const std::vector<Detection>&) override {
        return std::nullopt;  // Not implemented
    }
};

// Recoil compensation (not implemented in MVP)
class IRecoilCompensator {
public:
    virtual ~IRecoilCompensator() = default;
    virtual Vec2 compensate(Vec2 baseMovement, bool isShooting) = 0;
};

class NullRecoilCompensator : public IRecoilCompensator {
    Vec2 compensate(Vec2 baseMovement, bool) override {
        return baseMovement;  // No compensation
    }
};
```

**Input Thread Hook (Ready for Future Features):**
```cpp
void inputLoop() {
    auto recoilComp = factory.create<IRecoilCompensator>("Null");  // MVP stub

    while (running) {
        AimCommand cmd = latestCommand.load();
        Vec2 movement = trajectoryPlanner.step(cmd);

        // HOOK: Future recoil compensation
        movement = recoilComp->compensate(movement, isMouseDown());

        driver->move(movement.x, movement.y);
    }
}
```

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)

**Goal:** Core infrastructure and basic pipeline

- [ ] CMake build system setup
- [ ] Core interfaces defined (`IScreenCapture`, `IDetector`, `IMouseDriver`)
- [ ] Dependency integration (OpenCV, ONNX Runtime, ImGui)
- [ ] Lock-free queue implementation (LatestFrameQueue)
- [ ] Thread manager (create, pause, resume, shutdown)
- [ ] Logging system (spdlog)
- [ ] Basic unit tests (Catch2 setup)

**Deliverable:** Compiling project with skeleton implementations

---

### Phase 2: Capture & Detection (Weeks 3-4)

**Goal:** Working screen capture → AI inference

- [ ] WinrtCapture implementation
- [ ] DuplicationCapture fallback
- [ ] Texture pool (triple buffer with ref-counting)
- [ ] DMLDetector implementation (DirectML backend)
- [ ] TensorRTDetector implementation (NVIDIA backend)
- [ ] NMS post-processing
- [ ] Detection→Tracking message passing

**Deliverable:** Headless demo detecting targets in recorded gameplay

**Testing:**
- [ ] Unit tests for NMS algorithm
- [ ] Integration test with fake capture (pre-recorded frames)
- [ ] Benchmark: 640x640 inference < 10ms on RTX 3070 Ti

---

### Phase 3: Tracking & Prediction (Week 5)

**Goal:** Multi-target tracking with Kalman filtering

- [ ] TargetDatabase (SoA structure)
- [ ] Data association (match detections to tracks)
- [ ] Kalman filter implementation
- [ ] Target selection logic (FOV, distance, hitbox priority)
- [ ] AimCommand structure
- [ ] Unified timestamping

**Deliverable:** System selects best target and outputs aim coordinates

**Testing:**
- [ ] Unit tests for Kalman prediction accuracy
- [ ] Integration test: tracking moving targets across frames

---

### Phase 4: Input & Aiming (Week 6)

**Goal:** Smooth mouse control

- [ ] Win32Driver (SendInput)
- [ ] ArduinoDriver (serial communication)
- [ ] Bezier curve trajectory planning
- [ ] 1 Euro filter smoothing
- [ ] Input thread (1000Hz loop)
- [ ] TrajectoryPlanner configuration (smoothness parameter)

**Deliverable:** End-to-end aimbot (capture → aim → mouse move)

**Testing:**
- [ ] Unit tests for Bezier curve (no overshoot, smooth interpolation)
- [ ] Manual testing: smoothness feels human-like
- [ ] Latency measurement: capture → mouse move < 10ms

---

### Phase 5: Configuration & Auto-Detection (Week 7)

**Goal:** Game profiles and auto-switching

- [ ] GameProfile JSON schema
- [ ] ProfileManager (load, validate, switch)
- [ ] GameDetector (polling with hysteresis)
- [ ] ModelManager (single model, lazy load)
- [ ] SharedConfig (memory-mapped IPC)
- [ ] Config validation with fallbacks

**Deliverable:** Auto-detects game, loads profile, switches models

**Testing:**
- [ ] Unit tests for JSON parsing with invalid input
- [ ] Integration test: switch between 2 games (Valorant, CS2)
- [ ] Verify no VRAM leak on model switch

---

### Phase 6: UI & Observability (Week 8)

**Goal:** Debug overlay and external config app

- [ ] In-game overlay (ImGui transparent window)
  - Performance metrics
  - Bounding box visualization
  - Component toggles
- [ ] External config UI (standalone ImGui app)
  - Profile editor
  - Live tuning sliders
  - Telemetry dashboard
- [ ] FrameProfiler (latency graphs)
- [ ] BottleneckDetector (suggestions)

**Deliverable:** Full UI for debugging and configuration

**Testing:**
- [ ] Manual testing: overlay doesn't impact game performance
- [ ] Verify IPC: slider changes reflected instantly (<10ms)

---

### Phase 7: Testing & Benchmarking (Week 9)

**Goal:** Comprehensive test suite

- [ ] Complete unit test coverage for algorithms
- [ ] Integration tests with golden datasets
- [ ] CLI benchmark tool (`sunone-bench.exe`)
- [ ] Record test datasets (500 frames per game)
- [ ] CI/CD pipeline (GitHub Actions)
- [ ] Performance regression tests

**Deliverable:** Automated test suite with CI/CD

**Testing:**
- [ ] All tests pass on clean build
- [ ] Benchmark meets targets (120+ FPS, <10ms latency)

---

### Phase 8: Optimization & Polish (Week 10)

**Goal:** Meet performance targets

- [ ] Profile with Tracy/VTune
- [ ] Optimize bottlenecks (SIMD, memory access patterns)
- [ ] Thread affinity tuning
- [ ] GPU resource optimization
- [ ] Memory pool tuning
- [ ] Error handling and recovery

**Deliverable:** Production-ready MVP

**Testing:**
- [ ] Stress test: 1-hour continuous operation
- [ ] Multi-game testing (3+ games)
- [ ] Performance validation on different GPUs

---

### Phase 9: Documentation (Week 11)

**Goal:** User and developer documentation

- [ ] User guide (installation, configuration, troubleshooting)
- [ ] Developer guide (architecture, building, contributing)
- [ ] API documentation (Doxygen)
- [ ] Video tutorials (basic usage, advanced tuning)

**Deliverable:** Complete documentation

---

### Phase 10: Release (Week 12)

**Goal:** Public MVP release

- [ ] Final testing on clean systems
- [ ] Package installer (include dependencies)
- [ ] GitHub release (binaries + source)
- [ ] Community support channels

**Deliverable:** v1.0.0 MVP Release

---

## Appendix

### A. Technology Stack Details

| Component | Library/Framework | Version | License |
|-----------|-------------------|---------|---------|
| **Language** | C++ | C++20 | - |
| **Build System** | CMake | 3.25+ | BSD-3 |
| **Computer Vision** | OpenCV | 4.10.0 | Apache 2.0 |
| **AI Inference** | ONNX Runtime | 1.19+ | MIT |
| **DirectML** | Windows SDK | 10.0.22621+ | MS-PL |
| **TensorRT** | NVIDIA TensorRT | 10.8.0 | NVIDIA |
| **UI** | Dear ImGui | 1.91.2 | MIT |
| **Window Management** | GLFW | 3.4 | Zlib |
| **Threading** | moodycamel::ConcurrentQueue | Latest | BSD-2 |
| **Unit Testing** | Catch2 | 3.x | BSL-1.0 |
| **Benchmarking** | Google Benchmark | 1.8+ | Apache 2.0 |
| **Logging** | spdlog | 1.14+ | MIT |
| **JSON** | nlohmann/json | 3.11+ | MIT |
| **Linear Algebra** | Eigen | 3.4+ | MPL2 |

### B. Build Instructions

**Prerequisites:**
- Windows 10/11 (1903+)
- Visual Studio 2022 with C++20 support
- CMake 3.25+
- CUDA Toolkit 12.8 (for TensorRT, optional)

**Build Steps:**
```bash
# Clone repository
git clone https://github.com/yourusername/marcoman_ai_aimbot.git
cd marcoman_ai_aimbot

# Configure CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j

# Run tests
ctest --test-dir build -C Release

# Install
cmake --install build --prefix install
```

**CMake Options:**
- `-DENABLE_TENSORRT=ON`: Build with TensorRT support (requires CUDA)
- `-DENABLE_TESTS=ON`: Build unit tests (default: ON)
- `-DENABLE_BENCHMARKS=ON`: Build benchmark suite (default: OFF)

### C. Directory Structure

```
marcoman_ai_aimbot/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
│
├── docs/
│   ├── plans/
│   │   └── 2025-12-29-modern-aimbot-architecture-design.md  (this file)
│   ├── user-guide.md
│   └── developer-guide.md
│
├── extracted_modules/          # Core SDK modules
│   ├── capture/
│   │   ├── WinrtCapture.h/cpp
│   │   └── DuplicationCapture.h/cpp
│   ├── detection/
│   │   ├── directml/DMLDetector.h/cpp
│   │   ├── tensorrt/TensorRTDetector.h/cpp
│   │   └── postprocess/PostProcessor.h/cpp
│   ├── input/
│   │   ├── drivers/
│   │   │   ├── Win32Driver.h/cpp
│   │   │   └── ArduinoDriver.h/cpp
│   │   ├── movement/
│   │   │   ├── BezierCurve.h
│   │   │   ├── OneEuroFilter.h
│   │   │   └── TrajectoryPlanner.h/cpp
│   │   └── prediction/
│   │       └── KalmanPredictor.h/cpp
│   ├── core/
│   │   ├── interfaces/
│   │   │   ├── IScreenCapture.h
│   │   │   ├── IDetector.h
│   │   │   ├── IMouseDriver.h
│   │   │   └── IPredictor.h
│   │   ├── entities/
│   │   │   ├── Frame.h
│   │   │   ├── Detection.h
│   │   │   ├── Target.h
│   │   │   └── AimCommand.h
│   │   ├── threading/
│   │   │   ├── ThreadSafeQueue.h
│   │   │   └── LatestFrameQueue.h
│   │   └── utils/
│   │       └── PathUtils.h
│   └── config/
│       ├── AppConfig.h
│       └── GameProfile.h
│
├── src/                        # Main application
│   ├── main.cpp
│   ├── Engine.h/cpp            # Main engine orchestrator
│   ├── Pipeline.h/cpp          # Pipeline thread management
│   ├── ComponentFactory.h/cpp
│   ├── ModelManager.h/cpp
│   ├── ProfileManager.h/cpp
│   └── GameDetector.h/cpp
│
├── ui/                         # UI applications
│   ├── overlay/
│   │   ├── DebugOverlay.h/cpp
│   │   └── FrameProfiler.h/cpp
│   └── config_app/
│       └── ConfigApp.cpp       # Standalone config UI
│
├── tests/
│   ├── unit/
│   │   ├── test_kalman.cpp
│   │   ├── test_bezier.cpp
│   │   └── test_nms.cpp
│   ├── integration/
│   │   └── test_pipeline.cpp
│   └── benchmark/
│       └── sunone-bench.cpp    # CLI benchmark tool
│
├── test_data/                  # Golden datasets
│   ├── valorant_500frames.bin
│   └── cs2_500frames.bin
│
├── config/                     # Runtime configuration
│   ├── global.ini
│   ├── plugins.ini
│   └── games/
│       ├── valorant.json
│       └── cs2.json
│
├── models/                     # ONNX models
│   ├── valorant_yolov8_640.onnx
│   └── cs2_yolov8_640.onnx
│
└── modules/                    # External dependencies
    ├── opencv/
    ├── onnxruntime/
    ├── imgui/
    ├── glfw/
    └── TensorRT/
```

### D. Key Design Decisions & Rationale

**1. Why Lock-Free Queues Over Mutexes?**
- Research shows [lock-free queues outperform blocking queues](https://medium.com/@amansri99/benchmarking-lock-free-and-blocking-concurrent-queues-a-deep-dive-into-implementation-and-bf9a2b5c5d10) for 1-8 threads (our use case)
- Sub-microsecond latency in real-time sensor pipelines
- No priority inversion or contention issues

**2. Why SoA (Structure-of-Arrays) Over AoS?**
- **Cache Efficiency**: Iterating positions to find closest target only loads position data
- **SIMD Potential**: AVX2 can process 4-8 floats in parallel
- **Benchmarking**: 2-3x faster for typical "find closest" operations

**3. Why Single Model (No Preloading)?**
- **VRAM Budget**: YOLO models take 200-400MB each. 3 models = budget exceeded
- **Simplicity**: Lazy loading is easier to debug and reason about
- **Reality**: Game switches are rare (minutes/hours apart), loading time acceptable

**4. Why Message Passing Over Shared TargetDatabase?**
- **Zero Locks**: Detection and Tracking threads never contend
- **Ownership**: Tracking thread owns data, clearer lifecycle
- **Testability**: Easier to mock and test in isolation

**5. Why Hysteresis (3-second delay) on Game Switch?**
- **User Experience**: Alt-tabbing to Discord shouldn't trigger model reload
- **Performance**: Prevents thrashing when rapidly switching windows
- **Stability**: Only commits switch when game is genuinely active

### E. References & Research

**Performance & Threading:**
- [Real-Time Computer Vision Inference Latency](https://blog.roboflow.com/inference-latency/)
- [YOLO Thread-Safe Inference Guide](https://docs.ultralytics.com/guides/yolo-thread-safe-inference/)
- [Lock-Free Queue Performance Analysis](https://medium.com/@amansri99/benchmarking-lock-free-and-blocking-concurrent-queues-a-deep-dive-into-implementation-and-bf9a2b5c5d10)
- [moodycamel::ConcurrentQueue - Fast Lock-Free Queue for C++](https://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++)

**GPU Optimization:**
- [Computer Vision Pipeline Optimization with GPU Computing](https://www.runpod.io/articles/guides/computer-vision-pipeline-optimization-accelerating-image-processing-workflows-with-gpu-computing)
- [GPU Sharing Strategies - Time-Slicing](https://www.redhat.com/en/blog/sharing-caring-how-make-most-your-gpus-part-1-time-slicing)
- [Optimize AI Inference Performance with GPUs](https://contentbase.com/blog/optimizing-ai-inference-performance-gpu/)

**Algorithm References:**
- 1 Euro Filter: [Casiez et al., CHI 2012](https://cristal.univ-lille.fr/~casiez/1euro/)
- Kalman Filter: [Welch & Bishop, UNC-Chapel Hill](https://www.cs.unc.edu/~welch/kalman/)
- Non-Maximum Suppression: [Neubeck & Van Gool, ICPR 2006]

### F. Glossary

| Term | Definition |
|------|------------|
| **AoS** | Array of Structures (e.g., `std::vector<Target>`) |
| **Backpressure** | Queue policy when consumer is slower than producer |
| **DML** | DirectML (Microsoft's GPU acceleration API) |
| **ECS** | Entity-Component-System architecture |
| **EMA** | Exponential Moving Average (for smoothing metrics) |
| **FOV** | Field of View (circular region for target selection) |
| **Head-Drop** | Policy to drop old frames, always process latest |
| **Hysteresis** | Delay before committing state change (debounce) |
| **IPC** | Inter-Process Communication |
| **Latency** | Time delay from input to output |
| **Lock-Free** | Concurrent algorithm using atomics, no mutexes |
| **NMS** | Non-Maximum Suppression (remove overlapping detections) |
| **ONNX** | Open Neural Network Exchange (model format) |
| **P95/P99** | 95th/99th percentile (worst-case performance) |
| **SoA** | Structure of Arrays (e.g., separate vectors for x, y) |
| **VRAM** | Video RAM (GPU memory) |
| **Zero-Copy** | Data stays on GPU, no CPU↔GPU transfer |

### G. Contact & Support

**Developer:** Marcoman (therouxe)
**Repository:** [GitHub - marcoman_ai_aimbot](https://github.com/yourusername/marcoman_ai_aimbot)
**License:** TBD
**Issues:** [GitHub Issues](https://github.com/yourusername/marcoman_ai_aimbot/issues)

---

**Document Status:** ✅ Complete (Ready for Implementation)
**Next Steps:** Begin Phase 1 implementation (Foundation)

---

*Generated with [Claude Code](https://claude.com/claude-code) - Architecture Design Session*
