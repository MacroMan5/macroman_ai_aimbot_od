# Critical Implementation Traps

**READ THIS BEFORE CODING** - These are proven failure modes from the original SunONE project.

---

## Trap 1: The "Leak on Drop" Trap

**Location:** `LatestFrameQueue::push()`

**Problem:**
```cpp
void push(Frame* frame) {
    Frame* old = slot.exchange(frame, std::memory_order_release);
    if (old) delete old;  // ⚠️ DANGER: Frame holds Texture* from pool!
}
```

If `Frame` contains a `Texture*` from the pool and you just `delete old`, the texture's refCount is **never decremented**. Result: **TexturePool starves within 3 frames** (all textures marked busy).

**Fix: RAII with Custom Deleter**
```cpp
struct TextureDeleter {
    TexturePool* pool;
    void operator()(Texture* tex) const {
        if (pool && tex) pool->release(tex);  // ✅ Decrement refCount
    }
};

struct Frame {
    std::unique_ptr<Texture, TextureDeleter> texture;
    std::chrono::high_resolution_clock::time_point captureTime;
    uint64_t frameSequence;

    Frame(Texture* tex, TexturePool* pool)
        : texture(tex, TextureDeleter{pool}) {}
};

// Now LatestFrameQueue::push() automatically releases texture on delete
void push(std::unique_ptr<Frame> new_frame) {
    auto* old = slot.exchange(new_frame.release(), std::memory_order_release);
    if (old) delete old;  // ✅ SAFE - Frame destructor calls pool->release()
}
```

**Validation:**
```cpp
// In Capture thread loop:
assert(texturePool.getAvailableCount() > 0 && "TexturePool starved!");

// Add metric:
metrics.texturePoolStarved.fetch_add(1);
```

**Symptom if violated:** Capture thread hangs after 3-4 frames, all textures marked as "in use".

---

## Trap 2: Input Extrapolation Overshoot

**Location:** Input Thread prediction logic

**Problem:**
```cpp
// Dangerous: unbounded extrapolation
Vec2 predictedPos = target.position + target.velocity * (now - target.observedAt).count();
```

If Detection thread stutters (Windows Update, GPU driver update, etc.), `dt` can exceed **100ms**:
- Target predicted off-screen → mouse snaps to screen edge
- Instant snap speed = detectable by anti-cheat
- User sees erratic behavior

**Fix: Prediction Clamping + Confidence Decay**
```cpp
float dt = std::chrono::duration<float>(now - target.observedAt).count();
dt = std::min(dt, 0.050f);  // ✅ Cap prediction at 50ms

if (dt > 0.050f) {
    // Data too stale (>50ms old) - decay confidence
    cmd.confidence *= 0.5f;

    if (cmd.confidence < 0.3f) {
        cmd.hasTarget = false;  // ✅ Stop aiming
        LOG_WARN("Target data too stale ({:.1f}ms), disabling aim", dt * 1000.0f);
        return;
    }
}

Vec2 predictedPos = target.position + target.velocity * dt;  // ✅ Safe extrapolation
```

**Telemetry:**
```cpp
// Track stale prediction events
if (dt > 0.050f) {
    metrics.stalePredictionEvents.fetch_add(1);
}

// Alert if >10 events per minute
if (stalePredictionEvents > 10) {
    LOG_ERROR("Detection thread severely degraded ({} stale events)", stalePredictionEvents);
}
```

**Rationale:** 50ms is the maximum reasonable prediction horizon. Beyond that, predictions become unreliable and may cause harm.

**Symptom if violated:** Mouse occasionally snaps violently to screen edge during stutters.

---

## Trap 3: Shared Memory Atomics (Platform-Specific)

**Location:** `SharedConfig` structure (IPC between Engine and Config UI)

**Problem:** `std::atomic` is **NOT guaranteed** to be lock-free across different processes. If atomics use locks internally, deadlock is possible if one process crashes while holding the lock.

**Fix: Static Assertions + Alignment**
```cpp
struct SharedConfig {
    // Align to cache line to avoid false sharing
    alignas(64) std::atomic<float> aimSmoothness{0.5f};
    alignas(64) std::atomic<float> fov{60.0f};
    alignas(64) std::atomic<uint32_t> activeProfileId{0};
    alignas(64) std::atomic<bool> enablePrediction{true};

    // ✅ Static assertions: Ensure lock-free on this platform
    static_assert(decltype(aimSmoothness)::is_always_lock_free,
                  "std::atomic<float> MUST be lock-free for IPC safety");
    static_assert(decltype(fov)::is_always_lock_free,
                  "std::atomic<float> MUST be lock-free for IPC safety");
    static_assert(decltype(activeProfileId)::is_always_lock_free,
                  "std::atomic<uint32_t> MUST be lock-free for IPC safety");
    static_assert(decltype(enablePrediction)::is_always_lock_free,
                  "std::atomic<bool> MUST be lock-free for IPC safety");

    // ✅ Ensure both processes compile with same alignment
    static_assert(alignof(SharedConfig) == 64, "Alignment mismatch between processes");
};
```

**Runtime Verification:**
```cpp
// In Engine initialization:
LOG_INFO("SharedConfig lock-free verification:");
LOG_INFO("  atomic<float>:   {}", std::atomic<float>::is_always_lock_free);
LOG_INFO("  atomic<uint32_t>: {}", std::atomic<uint32_t>::is_always_lock_free);
LOG_INFO("  atomic<bool>:    {}", std::atomic<bool>::is_always_lock_free);

if (!std::atomic<float>::is_always_lock_free) {
    LOG_CRITICAL("Platform does not support lock-free atomic<float> - UNSAFE FOR IPC");
    return false;
}
```

**Platform Note:** On x64 Windows, `atomic<T>` is lock-free for `sizeof(T) <= 8` bytes. This covers `float`, `int`, `bool`, pointers.

**Symptom if violated:** Process crashes or hangs when other process terminates unexpectedly.

---

## Trap 4: Deadman Switch (Safety)

**Problem:** If Capture thread crashes, Input thread keeps running with stale data:
- Tracking "ghost" target (data from 5 seconds ago)
- Pulling down continuously (recoil control with no stop)
- Moving erratically

**Fix: Input Thread Timeout**
```cpp
void inputLoop() {
    auto lastCommandTime = high_resolution_clock::now();

    while (running) {
        AimCommand cmd = latestCommand.load(std::memory_order_acquire);
        auto now = high_resolution_clock::now();

        // ✅ Deadman switch: Stop if no new commands for 200ms
        auto staleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastCommandTime
        ).count();

        if (staleDuration > 200) {
            LOG_WARN("Input stale (no commands for {}ms) - EMERGENCY STOP", staleDuration);
            cmd.hasTarget = false;
            metrics.deadmanSwitchTriggered.fetch_add(1);

            // Optionally: Trigger emergency shutdown
            if (staleDuration > 1000) {
                LOG_CRITICAL("Input stale for >1s - shutting down");
                requestShutdown();
                break;
            }
        }

        if (cmd.hasTarget) {
            lastCommandTime = now;  // ✅ Update timestamp on valid command
            // Process input...
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

**Telemetry:**
```cpp
// Monitor deadman switch activation
if (metrics.deadmanSwitchTriggered > 0) {
    LOG_ERROR("Deadman switch triggered {} times - investigate Detection/Tracking health",
              metrics.deadmanSwitchTriggered.load());
}
```

**Symptom if violated:** Mouse moves erratically or continues aiming after Capture/Detection crashes.

---

## Trap 5: Detection Batch Allocation

**Problem:** `ObjectPool<DetectionBatch>` recycles the batch object, but `std::vector<Detection>` inside still allocates its internal buffer on the heap every frame.

**Issue:**
```cpp
struct DetectionBatch {
    std::vector<Detection> observations;  // ⚠️ Allocates heap buffer every time
    std::chrono::time_point timestamp;
};

// Recycling the batch doesn't recycle the vector's buffer
pool.release(batch);  // ❌ batch object recycled, but vector buffer NOT reused
```

**Fix: Fixed-Capacity Vector (Pre-Allocated)**
```cpp
template<typename T, size_t Capacity>
class FixedCapacityVector {
    std::array<T, Capacity> data_;
    size_t count_{0};

public:
    void push_back(const T& item) {
        assert(count_ < Capacity && "FixedCapacityVector overflow");
        data_[count_++] = item;
    }

    void clear() { count_ = 0; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

    // Iterator support (for range-based for)
    T* begin() { return data_.data(); }
    T* end() { return data_.data() + count_; }
    const T* begin() const { return data_.data(); }
    const T* end() const { return data_.data() + count_; }
};

struct DetectionBatch {
    FixedCapacityVector<Detection, 64> observations;  // ✅ Pre-allocated, max 64 targets
    std::chrono::time_point timestamp;
};
```

**Validation: Zero Allocations in Hot Path**
```cpp
// Profile with Tracy/VTune - should show ZERO malloc/free calls
// in Detection→Tracking path after warmup
```

**Benefit:**
- No heap allocations in hot path (after initial setup)
- Better cache locality (contiguous memory)
- Predictable performance (no allocator variance)

**Symptom if violated:** Profiler shows frequent `malloc`/`free` calls in Detection thread, causing jitter.

---

## Summary Checklist

**Before committing threading/memory code, verify:**

- [ ] **Trap 1:** Frame uses RAII deleter for TexturePool
- [ ] **Trap 2:** Prediction capped at 50ms, confidence decay on stale data
- [ ] **Trap 3:** SharedConfig atomics verified lock-free with `static_assert`
- [ ] **Trap 4:** Input thread has deadman switch (200ms timeout)
- [ ] **Trap 5:** DetectionBatch uses `FixedCapacityVector`, not `std::vector`

**Validation commands:**
```bash
# Profile for allocations (should be zero in hot path)
tracy-profiler ./build/bin/macroman_aimbot.exe

# Run sanitizers
cmake -DENABLE_ASAN=ON -DENABLE_TSAN=ON ..
./build/bin/unit_tests
```

---

**Last updated:** 2025-12-30
**Referenced by:** `@CLAUDE.md`
