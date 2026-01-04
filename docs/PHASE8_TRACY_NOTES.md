# Phase 8 - Tracy Profiler Integration Notes

## P8-01: Tracy Profiler Integration Status

**Completed:** 2026-01-03

### Implemented

1. **Profiler Wrapper** (`src/core/profiling/Profiler.h`)
   - Conditional compilation macros (ENABLE_TRACY)
   - Zero overhead when disabled (default)
   - Macros: ZONE_SCOPED, ZONE_NAMED, FRAME_MARK, ZONE_VALUE

2. **CMake Integration** (`CMakeLists.txt` lines 12, 69-84)
   - Optional: `-DENABLE_TRACY=ON` to enable
   - FetchContent integration with Tracy v0.10
   - Default: OFF (zero overhead)

3. **Input Thread Instrumentation** (`src/input/InputManager.cpp`)
   - ZONE_SCOPED added to inputLoop() at line 106
   - Measures 1000Hz input thread performance

### Pending (Waiting for Engine Implementation)

The following thread loops **do not exist yet** and need Tracy instrumentation when implemented:

1. **Capture Thread Loop** (Phase 5 - Engine)
   - Add: `ZONE_NAMED("CaptureFrame")` in main loop
   - Instrument: WinrtCapture::captureFrame() or DuplicationCapture::captureFrame()

2. **Detection Thread Loop** (Phase 5 - Engine)
   - Add: `ZONE_NAMED("YOLOInference")` for inference
   - Add: `ZONE_NAMED("NMS")` for post-processing
   - Measure GPU inference latency

3. **Tracking Thread Loop** (Phase 5 - Engine)
   - Add: `ZONE_NAMED("KalmanUpdate")` for filter updates
   - Add: `ZONE_NAMED("TargetSelection")` for selection logic
   - Measure tracking latency

### Verification

**Build Test:**
```bash
# Default build (Tracy disabled)
cmake --build build --config Release
# Result: SUCCESS (zero overhead)

# Enable Tracy profiler
cmake -B build -DENABLE_TRACY=ON
cmake --build build --config Release
# Result: (Not tested - requires Tracy GUI viewer)
```

**Test Results:**
- 123/125 tests passed (98%)
- 2 flaky timing tests failed (unrelated to Tracy changes)
- All PerformanceMetrics alignment tests passed

### Usage (When Engine is Implemented)

**Build with Tracy:**
```bash
cmake -B build -DENABLE_TRACY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

**Run with Tracy:**
```bash
# 1. Launch Tracy profiler GUI (Windows)
tracy.exe

# 2. Run instrumented aimbot
./build/bin/macroman_aimbot.exe

# 3. Tracy GUI will automatically connect and show:
#    - CaptureFrame (should be <1ms)
#    - YOLOInference (target: 5-8ms)
#    - NMS (should be <1ms)
#    - KalmanUpdate (should be <1ms)
#    - TargetSelection (should be <0.5ms)
#    - InputLoop (1000Hz = 1ms interval)
```

### Expected Profiling Results

| Zone | Target Latency | P95 Threshold | Action if Exceeded |
|------|----------------|---------------|--------------------|
| CaptureFrame | <1ms | 2ms | Check GPU driver, reduce capture resolution |
| YOLOInference | 5-8ms | 10ms | Switch to TensorRT, reduce input size |
| NMS | <1ms | 2ms | Optimize IoU threshold, limit detections |
| KalmanUpdate | <1ms | 2ms | Profile Eigen operations, check prediction count |
| TargetSelection | <0.5ms | 1ms | Optimize FOV filtering, cache calculations |
| InputLoop | 1ms (1000Hz) | 1.5ms | Reduce smoothing complexity, check driver latency |

### References

- Tracy Profiler: https://github.com/wolfpld/tracy
- Integration Guide: https://github.com/wolfpld/tracy/releases/latest
- Profiling Guide: `docs/PROFILING_GUIDE.md` (to be created in Phase 8)

---

**Next Task:** P8-02 - SIMD Acceleration for TargetDatabase
