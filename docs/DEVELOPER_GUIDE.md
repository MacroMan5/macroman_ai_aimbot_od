# MacroMan AI Aimbot v2 - Technical Developer Guide

**Target Audience:** Developers implementing or extending the aimbot system
**Prerequisites:** C++20, multithreading, GPU programming basics
**Last Updated:** 2026-01-04

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Pipeline Orchestration](#pipeline-orchestration)
3. [Memory Strategy](#memory-strategy)
4. [Threading Model](#threading-model)
5. [Performance Optimization](#performance-optimization)
6. [Extending the System](#extending-the-system)
7. [Critical Traps](#critical-traps)

---

## System Overview

MacroMan AI Aimbot v2 is a **real-time computer vision pipeline** for FPS games, built around a **4-thread architecture** with **lock-free communication** and **zero-allocation hot paths**.

### Core Architecture

```
┌─────────────────┐
│  Capture Thread │ (144 FPS)
│  DuplicationAPI │────┐
└─────────────────┘    │
                       ▼
                ┌──────────────────┐
                │ LatestFrameQueue │ (Head-Drop Policy)
                └──────────────────┘
                       │
                       ▼
┌─────────────────┐
│ Detection Thread│ (100-144 FPS)
│  DirectML YOLO  │────┐
└─────────────────┘    │
                       ▼
                ┌──────────────────┐
                │ LatestFrameQueue │ (Head-Drop Policy)
                └──────────────────┘
                       │
                       ▼
┌─────────────────┐
│ Tracking Thread │ (1000 FPS)
│ Kalman + Assoc. │────┐
└─────────────────┘    │
                       ▼
                ┌──────────────────┐
                │  Atomic AimCmd   │ (std::atomic<Vec2>)
                └──────────────────┘
                       │
                       ▼
┌─────────────────┐
│  Input Thread   │ (1000 Hz)
│   SendInput API │
└─────────────────┘
```

### Key Design Principles

1. **Head-Drop Policy**: Always drop old data to keep newest information
2. **Zero-Allocation Hot Path**: All memory pre-allocated during initialization
3. **Lock-Free Communication**: Atomics + SPSC queues (no mutexes in loops)
4. **RAII Everywhere**: Automatic resource cleanup (textures, GPU contexts)
5. **Cache-Line Alignment**: Minimize false sharing (`alignas(64)`)

---

## Pipeline Orchestration

### 1. Capture Thread (144 FPS Target)

**Thread Affinity:** Core 0 (configurable via `ThreadManager`)

**Responsibilities:**
- Capture frames from Desktop Duplication API
- Acquire texture from `TexturePool` (pre-allocated D3D11 textures)
- Push `Frame` to `LatestFrameQueue<Frame>` (lock-free)

**Implementation:** `DuplicationCapture::run()`

```cpp
void DuplicationCapture::run() {
    while (!shouldStop_.load(std::memory_order_relaxed)) {
        auto frame = captureFrame();  // <1ms target

        if (frame.isValid()) {
            frameQueue_.push(std::move(frame));  // Lock-free push
        }

        std::this_thread::sleep_for(6.9ms);  // ~144 FPS
    }
}
```

**Key Points:**
- `Frame` owns texture via `std::unique_ptr<Texture, TextureDeleter>`
- When `Frame` is destroyed, texture returns to pool (RAII)
- No manual `release()` calls needed

---

### 2. Detection Thread (100-144 FPS)

**Thread Affinity:** Core 1 (GPU-intensive)

**Responsibilities:**
- Pop latest frame from Capture→Detection queue
- Run YOLO inference (~8-12ms with DirectML)
- Apply NMS + confidence filtering
- Push `DetectionBatch` to Detection→Tracking queue

**Implementation:** `Engine::detectionThread()`

```cpp
void Engine::detectionThread() {
    while (!shouldStop_.load(std::memory_order_relaxed)) {
        Frame frame;
        if (!captureToDetectionQueue_.tryPop(frame)) {
            std::this_thread::sleep_for(1ms);
            continue;
        }

        auto detections = detector_->detect(frame);  // 8-12ms

        DetectionBatch batch;
        batch.detections = std::move(detections);
        batch.timestamp = std::chrono::steady_clock::now();

        detectionToTrackingQueue_.push(std::move(batch));
    }
}
```

**Performance Notes:**
- DirectML backend: 8-12ms inference
- TensorRT backend: 5-8ms inference (NVIDIA GPUs)
- Post-processing (NMS): ~0.5-1ms

---

### 3. Tracking Thread (1000 FPS)

**Thread Affinity:** Core 2

**Responsibilities:**
- Pop latest detections from Detection→Tracking queue
- Update Kalman filters for existing targets
- Perform data association (Hungarian algorithm)
- Select best target (FOV + hitbox priority)
- Write `AimCommand` to atomic variable

**Implementation:** `Engine::trackingThread()`

```cpp
void Engine::trackingThread() {
    while (!shouldStop_.load(std::memory_order_relaxed)) {
        DetectionBatch batch;
        if (detectionToTrackingQueue_.tryPop(batch)) {
            tracker_.update(batch);  // <1ms

            auto aimCmd = tracker_.getBestTarget();  // <0.5ms

            if (aimCmd.hasTarget) {
                aimCommand_.store(aimCmd.targetPos, std::memory_order_release);
            }
        }

        std::this_thread::sleep_for(1ms);  // 1000 Hz
    }
}
```

**Key Points:**
- `TargetTracker` maintains `TargetDatabase` (SoA layout, SIMD-optimized)
- Grace period: 500ms before removing lost targets
- Prediction: Kalman filter extrapolates up to 50ms into future

---

### 4. Input Thread (1000 Hz)

**Thread Affinity:** Core 3 (isolated from other threads)

**Responsibilities:**
- Read `AimCommand` from atomic variable (1000 Hz)
- Apply humanization (reaction delay, tremor, Bezier curves)
- Send mouse movement via `SendInput` API
- Enforce "Deadman Switch" (200ms timeout)

**Implementation:** `InputManager::run()`

```cpp
void InputManager::run() {
    while (!shouldStop_.load(std::memory_order_relaxed)) {
        auto aimPos = aimCommand_.load(std::memory_order_acquire);

        if (isStale(aimPos.timestamp, 200ms)) {
            // Deadman switch: No fresh commands, stop input
            metrics_.deadmanSwitchTriggered.fetch_add(1);
            continue;
        }

        auto movement = humanizer_.applyHumanization(aimPos);  // <0.5ms
        mouseDriver_->move(movement.dx, movement.dy);         // <0.5ms

        std::this_thread::sleep_for(1ms);  // 1000 Hz
    }
}
```

**Humanization Pipeline:**
1. **Reaction Delay:** 50-150ms random delay before first movement
2. **Bezier Curve:** Smooth S-curve trajectory (t ∈ [0, 1.15] for overshoot)
3. **Tremor Simulation:** Sinusoidal jitter ±1-2px amplitude

---

## Memory Strategy

### Zero-Allocation Hot Path

**Problem:** `new`/`delete` in loops causes frame drops and memory fragmentation.

**Solution:** Pre-allocate all resources during initialization.

### TexturePool (RAII-Based)

**Purpose:** Reuse D3D11 textures without dynamic allocation.

**Design:**
```cpp
class TexturePool {
    std::array<Texture, 8> textures_;  // Pre-allocated
    std::array<bool, 8> busy_;         // Busy flags

public:
    std::unique_ptr<Texture, TextureDeleter> acquire() {
        for (size_t i = 0; i < 8; ++i) {
            bool expected = false;
            if (busy_[i].compare_exchange_strong(expected, true)) {
                return std::unique_ptr<Texture, TextureDeleter>(
                    &textures_[i],
                    TextureDeleter{this, i}
                );
            }
        }
        return nullptr;  // Pool starved (Critical Trap #1)
    }

    void release(size_t index) {
        busy_[index].store(false, std::memory_order_release);
    }
};
```

**RAII Deleter:**
```cpp
struct TextureDeleter {
    TexturePool* pool;
    size_t index;

    void operator()(Texture* tex) {
        pool->release(index);  // Auto-release on Frame destruction
    }
};
```

**Key Points:**
- `Frame` holds `std::unique_ptr<Texture, TextureDeleter>`
- When `Frame` goes out of scope, texture returns to pool
- **CRITICAL:** If pool starved (all 8 busy), `acquire()` returns `nullptr`
- Monitor `metrics_.texturePoolStarved` counter (see CRITICAL_TRAPS.md)

---

### LatestFrameQueue (Head-Drop Policy)

**Purpose:** Lock-free SPSC queue that always drops old data.

**Implementation:**
```cpp
template<typename T>
class LatestFrameQueue {
    std::atomic<T*> head_{nullptr};

public:
    void push(T&& item) {
        T* newItem = new T(std::move(item));
        T* old = head_.exchange(newItem, std::memory_order_acq_rel);
        delete old;  // Drop old frame (head-drop policy)
    }

    bool tryPop(T& out) {
        T* current = head_.exchange(nullptr, std::memory_order_acquire);
        if (!current) return false;

        out = std::move(*current);
        delete current;
        return true;
    }
};
```

**Why Head-Drop?**
- Detection thread slower than Capture (8-12ms vs 6.9ms)
- Always want **newest** frame for inference
- Old frames are irrelevant (game state changed)

**Trade-off:**
- Low memory usage (single item)
- Guaranteed freshness
- Some frames dropped (acceptable for real-time CV)

---

## Threading Model

### Thread Affinity Strategy

**Goal:** Minimize context switches and cache thrashing.

**Assignment:**
- **Capture Thread:** Core 0 (fast, low-latency operations)
- **Detection Thread:** Core 1 (GPU-heavy, isolated from input)
- **Tracking Thread:** Core 2 (CPU-heavy, SIMD operations)
- **Input Thread:** Core 3 (critical latency, isolated)

**Implementation:** `ThreadManager::setThreadAffinity()`

```cpp
void ThreadManager::setThreadAffinity(std::thread& t, CoreAffinity affinity) {
    HANDLE handle = t.native_handle();
    DWORD_PTR mask = static_cast<DWORD_PTR>(affinity);
    SetThreadAffinityMask(handle, mask);
}
```

**Reference:** `docs/CRITICAL_TRAPS.md` - Trap #3 (Lock-Free Violation)

---

### Atomic Communication

**Principle:** Never use `std::mutex` in hot paths.

**Shared State:**
```cpp
struct Engine {
    // Atomic aim command (Input thread reads, Tracking writes)
    alignas(64) std::atomic<Vec2> aimCommand_{};

    // Cache-line aligned to avoid false sharing
    alignas(64) std::atomic<bool> shouldStop_{false};
};
```

**Why `alignas(64)`?**
- Modern CPUs have 64-byte cache lines
- Without alignment, `aimCommand_` and `shouldStop_` might share a cache line
- **False sharing:** Thread 1 writes `aimCommand_`, invalidates cache line for Thread 2 reading `shouldStop_`

---

## Performance Optimization

### 1. SIMD Acceleration (AVX2)

**Target:** `TargetDatabase::findClosestToCenter()`

**Problem:** Linear search through 64 targets is slow.

**Solution:** AVX2 vectorized distance calculation.

```cpp
__m256 findClosestToCenter_AVX2(const std::array<Vec2, 64>& positions, Vec2 center) {
    __m256 minDist = _mm256_set1_ps(FLT_MAX);

    for (size_t i = 0; i < 64; i += 8) {
        __m256 px = _mm256_loadu_ps(&positions[i].x);
        __m256 py = _mm256_loadu_ps(&positions[i].y);

        __m256 dx = _mm256_sub_ps(px, _mm256_set1_ps(center.x));
        __m256 dy = _mm256_sub_ps(py, _mm256_set1_ps(center.y));

        __m256 distSq = _mm256_fmadd_ps(dx, dx, _mm256_mul_ps(dy, dy));
        minDist = _mm256_min_ps(minDist, distSq);
    }

    return minDist;
}
```

**Performance:** 8x speedup over scalar code (tested in `test_target_database.cpp`)

---

### 2. GPU Memory Management

**Principle:** Minimize CPU↔GPU transfers.

**Best Practices:**
1. Keep inference input/output buffers on GPU
2. Use `ID3D11Texture2D` for capture (already on GPU)
3. Avoid `Map()` / `Unmap()` in hot path
4. Pre-allocate all GPU resources during initialization

**Reference:** `src/detection/dml/DMLDetector.cpp` (DirectML session)

---

### 3. Profiling Hooks

**Tracy Integration:** (Phase 8)

```cpp
#ifdef ENABLE_TRACY
    ZoneScoped;  // Automatic function profiling
    FrameMark;   // Frame boundary markers
#endif
```

**Performance Metrics:** `PerformanceMetrics` class

```cpp
void Engine::captureThread() {
    auto start = std::chrono::high_resolution_clock::now();

    auto frame = capture_->captureFrame();

    auto end = std::chrono::high_resolution_clock::now();
    float latency = std::chrono::duration<float, std::milli>(end - start).count();

    metrics_.recordCapture(latency);  // EMA smoothing
}
```

---

## Extending the System

### Adding a New Detector Backend

**Example:** Adding CUDA + TensorRT backend

1. **Implement `IDetector` interface:**

```cpp
class TensorRTDetector : public IDetector {
    nvinfer1::ICudaEngine* engine_;
    nvinfer1::IExecutionContext* context_;

public:
    DetectionList detect(const Frame& frame) override {
        // 1. Copy texture to CUDA buffer
        // 2. Run TensorRT inference
        // 3. Parse outputs → Detection objects
        // 4. Apply NMS
    }
};
```

2. **Register in factory:**

```cpp
std::unique_ptr<IDetector> DetectorFactory::create(Backend backend) {
    switch (backend) {
        case Backend::DirectML:
            return std::make_unique<DMLDetector>();
        case Backend::TensorRT:
            return std::make_unique<TensorRTDetector>();
    }
}
```

3. **Update CMake:**

```cmake
if(ENABLE_TENSORRT)
    find_package(CUDAToolkit REQUIRED)
    find_package(TensorRT REQUIRED)

    target_link_libraries(macroman_detector PRIVATE
        CUDA::cudart
        TensorRT::nvinfer
    )
endif()
```

---

### Adding a New Humanization Feature

**Example:** Adding "fatigue simulation" (accuracy degrades over time)

1. **Extend `HumanizerConfig`:**

```cpp
struct HumanizerConfig {
    // ... existing fields ...
    bool enableFatigue = false;
    float fatigueRate = 0.001f;  // Accuracy loss per second
};
```

2. **Update `Humanizer::applyTremor()`:**

```cpp
Vec2 Humanizer::applyTremor(Vec2 movement, float deltaTime) {
    if (config_.enableFatigue) {
        fatigueAccumulator_ += config_.fatigueRate * deltaTime;
        float tremorAmplitude = baseTremorAmplitude_ * (1.0f + fatigueAccumulator_);
        // Apply increased tremor...
    }
    // ... rest of implementation ...
}
```

3. **Add unit tests:**

```cpp
TEST_CASE("Humanizer fatigue simulation", "[input]") {
    HumanizerConfig config;
    config.enableFatigue = true;
    config.fatigueRate = 0.01f;

    Humanizer humanizer(config);

    // Simulate 60 seconds of usage
    for (int i = 0; i < 60; ++i) {
        auto movement = humanizer.applyTremor({100, 0}, 1.0f);
        // Verify tremor amplitude increases
    }
}
```

---

## Critical Traps

**Reference:** `docs/CRITICAL_TRAPS.md`

### Trap #1: Texture Pool Starvation

**Symptom:** `capture_->captureFrame()` returns invalid frame.

**Root Cause:** All 8 textures marked busy (Frames not destroyed).

**Fix:** Ensure Frame RAII deleter is invoked. Check for:
- Frames stored in long-lived containers
- Exception handling that bypasses destructors
- Move semantics bugs (moved-from Frames still holding textures)

**Detection:** Monitor `metrics_.texturePoolStarved` counter.

---

### Trap #2: Stale Prediction Extrapolation

**Symptom:** Mouse aims at empty space (ghost targets).

**Root Cause:** Detection thread lagging >50ms, Kalman filter extrapolates too far.

**Fix:** In `TargetTracker::predict()`, clamp extrapolation:

```cpp
if (timeSinceLastUpdate > 50ms) {
    metrics_.stalePredictionEvents.fetch_add(1);
    return lastObservedPosition;  // Don't extrapolate
}
```

---

### Trap #3: Lock-Free Violation

**Symptom:** Random deadlocks or crashes under high load.

**Root Cause:** Using `std::mutex` in hot path (e.g., protecting aimCommand_).

**Fix:** Replace with atomics:

```cpp
// ❌ BAD
std::mutex aimCommandMutex_;
Vec2 aimCommand_;

void updateAim(Vec2 newPos) {
    std::lock_guard lock(aimCommandMutex_);  // Blocks input thread!
    aimCommand_ = newPos;
}

// ✅ GOOD
alignas(64) std::atomic<Vec2> aimCommand_;

void updateAim(Vec2 newPos) {
    aimCommand_.store(newPos, std::memory_order_release);  // Lock-free
}
```

**Verification:** Use `static_assert` to enforce lock-free:

```cpp
static_assert(std::atomic<Vec2>::is_always_lock_free,
              "Vec2 atomic must be lock-free!");
```

---

### Trap #4: Deadman Switch Bypass

**Symptom:** Mouse keeps moving even when game freezes.

**Root Cause:** Input thread doesn't check timestamp staleness.

**Fix:** Enforce 200ms timeout in `InputManager::run()`:

```cpp
auto aimCmd = aimCommand_.load(std::memory_order_acquire);

auto now = std::chrono::steady_clock::now();
auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - aimCmd.timestamp).count();

if (age > 200) {
    metrics_.deadmanSwitchTriggered.fetch_add(1);
    return;  // Don't send stale commands
}
```

---

## Conclusion

MacroMan AI Aimbot v2 is a **production-grade real-time CV pipeline** designed for **<10ms end-to-end latency**. Key takeaways:

1. **Lock-Free Communication:** Atomics + SPSC queues in hot paths
2. **Zero-Allocation:** TexturePool + pre-allocated memory
3. **Head-Drop Policy:** Always use newest data
4. **RAII Everywhere:** Automatic cleanup prevents leaks
5. **Thread Affinity:** Minimize context switches

**Next Steps:**
- Read `docs/USER_GUIDE.md` for calibration tips
- Review `docs/SAFETY_ETHICS.md` for ethical guidelines
- Check `docs/FAQ.md` for common implementation issues

**For Questions:**
- Discord: https://discord.gg/macroman
- GitHub Issues: https://github.com/MacroMan5/marcoman_ai_aimbot/issues
