# MacroMan AI Aimbot v2 - Troubleshooting & FAQ

**Target Audience:** Developers and users encountering common issues
**Last Updated:** 2026-01-04

---

## Table of Contents

1. [Common Implementation Traps](#common-implementation-traps)
2. [Component-Specific FAQ](#component-specific-faq)
3. [Performance Issues](#performance-issues)
4. [Build & Compilation](#build--compilation)
5. [Runtime Errors](#runtime-errors)
6. [Anti-Cheat & Detection](#anti-cheat--detection)

---

## Common Implementation Traps

These are **proven failure modes** from production use. If you encounter mysterious bugs, check here first.

### Trap #1: Texture Pool Starvation

**Reference:** `docs/CRITICAL_TRAPS.md` - Trap #1

**Symptoms:**
- Capture thread hangs after 3-4 frames
- Log shows: `[ERROR] TexturePool exhausted - all textures busy`
- Debug overlay frozen
- CPU usage drops to near-zero

**Root Cause:**
`Frame` objects not releasing textures back to pool when destroyed.

**Common Mistakes:**
```cpp
// ❌ BAD: Manual texture management
struct Frame {
    Texture* texture;  // Raw pointer - no RAII!

    ~Frame() {
        // Forgot to release texture back to pool!
    }
};

// ❌ BAD: Deleting Frame without calling pool->release()
void LatestFrameQueue::push(Frame* new_frame) {
    Frame* old = slot.exchange(new_frame);
    delete old;  // Texture still marked "busy" in pool!
}
```

**Correct Implementation:**
```cpp
// ✅ GOOD: RAII with custom deleter
struct TextureDeleter {
    TexturePool* pool;
    size_t index;

    void operator()(Texture* tex) const {
        if (pool) pool->release(index);  // Auto-release on destruction
    }
};

struct Frame {
    std::unique_ptr<Texture, TextureDeleter> texture;  // RAII ensures cleanup

    Frame(Texture* tex, TexturePool* pool, size_t idx)
        : texture(tex, TextureDeleter{pool, idx}) {}
};
```

**Debugging:**
```cpp
// Add telemetry counter
if (!texturePool.acquire()) {
    metrics_.texturePoolStarved.fetch_add(1);
    LOG_ERROR("TexturePool starved! Active: {}/{}", pool.getBusyCount(), pool.getCapacity());
}

// Check in debug overlay
if (metrics_.texturePoolStarved > 0) {
    ImGui::TextColored(RED, "⚠ TexturePool Starved: %llu", metrics_.texturePoolStarved);
}
```

**Fix:**
1. Search for all `new Frame(...)` calls
2. Verify `std::unique_ptr` with `TextureDeleter` is used
3. Ensure `Frame` move semantics don't leave textures unreleased
4. Monitor `metrics_.texturePoolStarved` counter

---

### Trap #2: Stale Prediction Extrapolation

**Reference:** `docs/CRITICAL_TRAPS.md` - Trap #2

**Symptoms:**
- Mouse snaps to random screen edges
- Aiming at "ghost targets" (empty space)
- Erratic movement during lag spikes
- Log shows: `stalePredictionEvents` counter increasing

**Root Cause:**
Detection thread lags >50ms, Kalman filter extrapolates target position too far into the future.

**Scenario:**
```
Frame 0:  Target at (500, 500), velocity = (10, 0) px/frame
Frame 1:  Detection delayed 100ms → Extrapolates: 500 + (10 × 100ms / 16ms) = 562px
          ⚠️ Target actually at (510, 500) → 52px error!
```

**Fix:**
```cpp
void TargetTracker::predict(Target& target) {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - target.lastUpdateTime).count();

    // ✅ Clamp extrapolation to 50ms max
    if (dt > 0.050f) {
        metrics_.stalePredictionEvents.fetch_add(1);
        dt = 0.050f;  // Don't predict beyond 50ms
    }

    target.predictedPos = target.pos + target.velocity * dt;
}
```

**Prevention:**
1. Monitor Detection thread latency in debug overlay
2. If latency > 20ms regularly, reduce model resolution
3. Enable TensorRT backend for faster inference
4. Set `maxExtrapolation = 50` in `config/global.ini`

---

### Trap #3: Lock-Free Violation

**Reference:** `docs/CRITICAL_TRAPS.md` - Trap #3

**Symptoms:**
- Random deadlocks (threads hang indefinitely)
- Input thread freezes during high CPU load
- Crash with `std::system_error: Resource deadlock would occur`

**Root Cause:**
Using `std::mutex` in hot path (called 1000+ times/second).

**Common Mistake:**
```cpp
// ❌ BAD: Mutex in input thread loop
std::mutex aimCommandMutex_;
AimCommand aimCommand_;

void InputManager::run() {
    while (!stop_) {
        {
            std::lock_guard lock(aimCommandMutex_);  // 1000 Hz locking!
            AimCommand cmd = aimCommand_;
        }
        processAimCommand(cmd);
    }
}
```

**Correct Implementation:**
```cpp
// ✅ GOOD: Lock-free atomic
alignas(64) std::atomic<AimCommand> aimCommand_;  // Cache-line aligned

void InputManager::run() {
    while (!stop_) {
        AimCommand cmd = aimCommand_.load(std::memory_order_acquire);  // Lock-free
        processAimCommand(cmd);
    }
}
```

**Verification:**
```cpp
// Compile-time check
static_assert(std::atomic<AimCommand>::is_always_lock_free,
              "AimCommand must be lock-free for real-time performance!");
```

**Debugging:**
```bash
# Check for mutex contention
# On Linux: perf record -e syscalls:sys_enter_futex ./macroman_aimbot
# On Windows: Use VTune or Tracy profiler

# If you see high futex/WaitForSingleObject counts → mutex problem
```

---

### Trap #4: Deadman Switch Bypass

**Reference:** `docs/CRITICAL_TRAPS.md` - Trap #4

**Symptoms:**
- Mouse continues moving after game freezes
- Aiming at outdated positions
- Input thread doesn't stop when Detection/Tracking crashes

**Root Cause:**
Input thread doesn't verify timestamp freshness before sending commands.

**Dangerous Code:**
```cpp
// ❌ BAD: No staleness check
void InputManager::run() {
    while (!stop_) {
        AimCommand cmd = aimCommand_.load();
        mouseDriver_->move(cmd.dx, cmd.dy);  // Sends stale commands!
    }
}
```

**Correct Implementation:**
```cpp
// ✅ GOOD: 200ms timeout enforced
void InputManager::run() {
    while (!stop_) {
        AimCommand cmd = aimCommand_.load(std::memory_order_acquire);

        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - cmd.timestamp).count();

        if (age > 200) {
            // Command older than 200ms → STOP INPUT
            metrics_.deadmanSwitchTriggered.fetch_add(1);
            LOG_WARN("Deadman switch triggered (command age: {}ms)", age);
            continue;  // Don't send movement
        }

        mouseDriver_->move(cmd.dx, cmd.dy);
    }
}
```

**Telemetry:**
```cpp
// Monitor in debug overlay
if (metrics_.deadmanSwitchTriggered > 0) {
    ImGui::TextColored(ORANGE, "⚠ Deadman Switch: %llu events", metrics_.deadmanSwitchTriggered);
}
```

---

## Component-Specific FAQ

### Capture (Desktop Duplication API)

#### Q: "Capture returns black frames"

**Symptoms:**
- `Frame` valid but texture contains all black pixels
- Log shows: `Capture successful` but detection finds nothing

**Causes:**
1. **Wrong monitor selected** - Capturing secondary monitor while game on primary
2. **Game using exclusive fullscreen** - DDA can't capture exclusive mode
3. **HDR mismatch** - Game in HDR mode but capture expects SDR

**Solutions:**
```cpp
// 1. Verify monitor selection
LOG_INFO("Capturing monitor: {} ({}x{})", monitor.name, monitor.width, monitor.height);

// 2. Force game to borderless windowed mode
//    Settings → Graphics → Display Mode → Borderless Window

// 3. Disable HDR in game settings if enabled
```

#### Q: "Capture latency >2ms (target <1ms)"

**Causes:**
1. Outdated GPU drivers
2. Game running at lower priority than capture thread
3. WDDM GPU scheduler disabled

**Solutions:**
```bash
# 1. Update GPU drivers
nvidia-smi  # Check version, update to 531.0+

# 2. Increase capture thread priority (Windows)
SetThreadPriority(captureThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);

# 3. Enable Windows GPU scheduler
# Settings → System → Display → Graphics → Hardware-accelerated GPU scheduling → On
```

---

### Detection (DirectML / TensorRT)

#### Q: "DirectML backend initialization failed"

**Symptoms:**
```
[ERROR] Failed to create DirectML device
[ERROR] D3D12CreateDevice returned E_INVALIDARG
```

**Causes:**
1. DirectX 12 not supported by GPU
2. Incompatible GPU driver
3. ONNX Runtime DirectML provider not found

**Solutions:**
```bash
# 1. Verify DirectX 12 support
dxdiag
# Check: Feature Levels → Should show 12_0 or higher

# 2. Reinstall ONNX Runtime with DirectML
# Download: https://github.com/microsoft/onnxruntime/releases
# Ensure "DirectML" variant (not CPU-only)

# 3. Fallback to CPU mode (slow)
cmake -B build -S . -DENABLE_TENSORRT=OFF
cmake --build build
```

#### Q: "Detection latency >20ms (target 8-12ms)"

**Causes:**
1. Model resolution too high (640x640)
2. Post-processing (NMS) taking too long
3. GPU thermal throttling

**Solutions:**
```ini
# 1. Reduce input resolution in global.ini
[Detection]
DetectionResolution = 0.5  # 640x640 → 320x320 (4x faster)

# 2. Increase NMS threshold (fewer boxes to process)
NMSThreshold = 0.5  # Default: 0.4 (higher = more aggressive filtering)

# 3. Check GPU temperature
nvidia-smi --query-gpu=temperature.gpu --format=csv
# If >85°C → clean dust, improve cooling
```

#### Q: "TensorRT vs DirectML - which is faster?"

| Backend | Inference Time | Requirements | Compatibility |
|---------|----------------|--------------|---------------|
| **DirectML** | 8-12ms | Any DX12 GPU | NVIDIA, AMD, Intel |
| **TensorRT** | 5-8ms | NVIDIA GPU | NVIDIA only |

**Recommendation:**
- NVIDIA RTX 30xx+ → Use TensorRT (40% faster)
- AMD GPUs → Use DirectML (only option)
- Intel Arc GPUs → Use DirectML

---

### Tracking (Kalman Filter + Data Association)

#### Q: "Tracker loses target frequently"

**Symptoms:**
- Target ID changes every 2-3 seconds
- Bounding box color switches (not green anymore)
- Jittery aim (constantly re-acquiring)

**Causes:**
1. `gracePeriod` too short
2. IoU threshold too strict
3. Detection confidence fluctuating

**Solutions:**
```ini
# 1. Increase grace period in global.ini
[Tracking]
GracePeriod = 1000  # Default: 500ms → 1 second

# 2. Lower IoU threshold (more lenient matching)
# In TargetTracker::associate()
constexpr float IOU_THRESHOLD = 0.3f;  // Default: 0.5 (lower = more matches)

# 3. Smooth confidence with EMA
float smoothedConf = target.confidence * 0.9f + detection.confidence * 0.1f;
```

#### Q: "Kalman filter predictions overshoot"

**Symptoms:**
- Crosshair aims ahead of target during sharp turns
- Overshoots corners when target changes direction

**Cause:**
Kalman process noise too low (assumes constant velocity).

**Solution:**
```cpp
// Increase process noise for more responsive tracking
KalmanFilter kalman;
kalman.Q = Eigen::Matrix4f::Identity() * 0.1f;  // Default: 0.01 (higher = more responsive)
```

---

### Input (SendInput API / Arduino Driver)

#### Q: "Input latency spikes >5ms (target <0.5ms)"

**Causes:**
1. Windows power plan set to "Balanced"
2. USB polling rate too low (125 Hz instead of 1000 Hz)
3. Game using raw input (bypasses SendInput)

**Solutions:**
```bash
# 1. Set power plan to "High Performance"
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c

# 2. Check USB polling rate (mouse settings)
# Most gaming mice: 1000 Hz default
# If lower → update firmware or mouse software

# 3. For games using raw input (Valorant, CS:GO):
#    → Use Arduino HID driver instead of SendInput
```

#### Q: "Arduino driver not detected"

**Symptoms:**
```
[ERROR] Failed to open serial port COM3
[ERROR] Device not found
```

**Solutions:**
```bash
# 1. Verify Arduino is plugged in
# Device Manager → Ports (COM & LPT) → Arduino Uno (COM3)

# 2. Install Arduino drivers
# https://www.arduino.cc/en/software

# 3. Update firmware on Arduino
cd tools/arduino_driver
arduino-cli upload --fqbn arduino:avr:uno --port COM3 mouse_emulator.ino

# 4. Test serial connection
echo "M 10 0" > COM3  # Should move mouse right 10px
```

---

## Performance Issues

### Q: "Overall latency >10ms (target <10ms)"

**Debugging Process:**

1. **Check per-thread latency in debug overlay (F1)**
   ```
   Capture:   0.8ms ✅
   Detection: 15.2ms ❌ BOTTLENECK
   Tracking:  0.4ms ✅
   Input:     0.3ms ✅
   ```

2. **Identify bottleneck thread** (the one exceeding target)

3. **Apply component-specific optimizations:**
   - Detection >15ms → Reduce model resolution or use TensorRT
   - Capture >2ms → Update GPU drivers
   - Tracking >2ms → Reduce `MAX_TARGETS` or disable prediction
   - Input >1ms → Check USB polling rate

---

### Q: "High CPU usage (>50% on 8-core system)"

**Expected CPU Usage:**
- Capture Thread: 5-10%
- Detection Thread: 20-30% (GPU-bound, minimal CPU)
- Tracking Thread: 10-15%
- Input Thread: 1-2%
- **Total:** ~40-50% (normal)

**If >70%:**
```cpp
// 1. Check for busy-wait loops
// ❌ BAD
while (!queue.tryPop(item)) {
    // Busy wait - wastes CPU
}

// ✅ GOOD
while (!queue.tryPop(item)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// 2. Verify thread affinity is set
// Threads competing for same core → context switching overhead

// 3. Profile with Tracy (if enabled)
#ifdef ENABLE_TRACY
    ZoneScoped;  // Identifies hot functions
#endif
```

---

### Q: "High VRAM usage (>1GB)"

**Expected VRAM Usage:**
- TexturePool: 8 × (1920×1080×4 bytes) = 64 MB
- YOLO Model: 12 MB
- DirectML Workspace: 200-400 MB
- **Total:** ~500 MB (normal)

**If >1GB:**
```cpp
// 1. Check for texture leaks
LOG_INFO("TexturePool busy: {}/{}", pool.getBusyCount(), pool.getCapacity());
// Should never be 8/8 for extended periods

// 2. Reduce TexturePool size (if really needed)
static constexpr size_t TEXTURE_POOL_SIZE = 4;  // Default: 8

// 3. Lower capture resolution
TargetFPS = 60  # 144 → 60 FPS (reduces capture resolution)
```

---

## Build & Compilation

### Q: "CMake can't find ONNX Runtime"

**Error:**
```
CMake Error: ONNX Runtime not found at C:/Users/.../.nuget/packages/...
```

**Solutions:**
```bash
# 1. Install via NuGet (Windows)
nuget install Microsoft.ML.OnnxRuntime.DirectML -Version 1.19.2

# 2. Manually specify path
cmake -B build -S . -DONNXRUNTIME_ROOT="C:/path/to/onnxruntime"

# 3. Use vcpkg (cross-platform)
vcpkg install onnxruntime
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

---

### Q: "Link error: unresolved external symbol for DirectML"

**Error:**
```
error LNK2019: unresolved external symbol DMLCreateDevice1
```

**Cause:**
DirectML library not linked.

**Fix:**
```cmake
# In src/detection/dml/CMakeLists.txt
target_link_libraries(macroman_detector PRIVATE
    DirectML  # ← Make sure this is present
    d3d12
    dxgi
)
```

---

## Runtime Errors

### Q: "Segmentation fault on startup"

**Common Causes:**
1. Null pointer dereference in initialization
2. Static initialization order fiasco
3. Unaligned atomic access

**Debugging:**
```bash
# 1. Run with debugger
gdb ./macroman_aimbot
(gdb) run
(gdb) bt  # Backtrace when crashed

# 2. Enable AddressSanitizer
cmake -B build -S . -DCMAKE_CXX_FLAGS="-fsanitize=address"
cmake --build build
./build/bin/macroman_aimbot  # Will show exact line of null deref

# 3. Check alignment
static_assert(alignof(SharedConfig) == 64, "Alignment violation!");
```

---

### Q: "Access violation writing location 0x00000000"

**Cause:**
Dereferencing null pointer (Windows equivalent of segfault).

**Common Locations:**
```cpp
// 1. Texture from pool is nullptr
Texture* tex = texturePool.acquire();
tex->width = 1920;  // ❌ CRASH if tex == nullptr

// Fix: Check for null
if (!tex) {
    LOG_ERROR("TexturePool exhausted!");
    return;
}

// 2. Uninitialized interface pointer
detector_->detect(frame);  // ❌ CRASH if detector_ is nullptr

// Fix: Initialize in constructor
Engine::Engine() : detector_(std::make_unique<DMLDetector>()) {}
```

---

## Anti-Cheat & Detection

### Q: "How detectable is this aimbot?"

**Detection Methods:**
1. **Statistical Analysis:** 🟠 MEDIUM RISK
   - Anti-cheat tracks aim accuracy over 100+ engagements
   - Humanization reduces but doesn't eliminate risk
   - **Mitigation:** Use `aimSmoothness > 0.5`, enable all humanization

2. **Pattern Recognition:** 🟡 LOW-MEDIUM RISK
   - ML models trained on human vs bot movement
   - Bezier curves + tremor make detection harder
   - **Mitigation:** Randomize humanization parameters per session

3. **Kernel-Level Monitoring:** 🔴 HIGH RISK (if enabled)
   - Vanguard, Easy Anti-Cheat can detect screen capture tools
   - SendInput API leaves traces in input log
   - **Mitigation:** None - use at your own risk in offline modes only

**Overall Risk:** 🟡 **MEDIUM** (detectable with sufficient data)

**Recommendation:** **NEVER use in online games with anti-cheat.**

---

### Q: "Can I make this undetectable?"

**Short Answer:** **No.** Any aimbot is eventually detectable given enough data.

**Long Answer:**
- **Statistical Outliers:** Perfect aim over 1000+ games is statistically impossible
- **Behavioral Patterns:** Even with humanization, patterns emerge over time
- **Kernel Detection:** Vanguard-level anti-cheat monitors all processes

**If you want "undetectable":**
- ❌ Don't build an aimbot
- ✅ Improve your skills legitimately
- ✅ Use this project for learning CV/AI concepts offline

---

## Still Having Issues?

### Community Support

- **GitHub Issues:** https://github.com/MacroMan5/marcoman_ai_aimbot/issues
- **Discord:** https://discord.gg/macroman
- **Reddit:** r/MacroManDev

### Before Asking for Help

1. **Check log file:** `logs/macroman_aimbot.log`
2. **Run with verbose logging:**
   ```bash
   .\macroman_aimbot.exe --log-level debug
   ```
3. **Include system info:**
   - OS version
   - GPU model + driver version
   - ONNX Runtime version
   - Full error message

### Reporting Bugs

**Good Bug Report:**
```
**Environment:**
- OS: Windows 11 23H2
- GPU: NVIDIA RTX 3060 (Driver 531.18)
- ONNX Runtime: 1.19.2 DirectML

**Steps to Reproduce:**
1. Launch Valorant in borderless windowed mode
2. Run `macroman_aimbot.exe`
3. Enter training range
4. Observe: Detection latency >30ms

**Expected:** Latency <12ms
**Actual:** Latency 35ms average

**Log Output:**
[2026-01-04 14:32:18] [ERROR] DirectML backend slow: 35.2ms
```

---

## Related Documentation

- **Technical Details:** `docs/DEVELOPER_GUIDE.md`
- **Setup Instructions:** `docs/USER_GUIDE.md`
- **Safety & Ethics:** `docs/SAFETY_ETHICS.md`
- **Critical Traps:** `docs/CRITICAL_TRAPS.md` (detailed trap analysis)

---

**Remember:** This is an educational project. If you're encountering issues in competitive online games, **you shouldn't be using this software there in the first place.** Use responsibly in offline modes only.
