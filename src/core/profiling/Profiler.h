/**
 * @file Profiler.h
 * @brief Tracy Profiler integration wrapper (Phase 8 - P8-01)
 *
 * Provides conditional compilation for Tracy profiler instrumentation.
 *
 * Usage:
 *   #include "core/profiling/Profiler.h"
 *
 *   void captureLoop() {
 *       while (running) {
 *           ZONE_SCOPED;  // Automatically names zone from function
 *           // or
 *           ZONE_NAMED("CaptureFrame");  // Custom zone name
 *
 *           // ... work ...
 *
 *           FRAME_MARK;  // Mark frame boundary (for FPS tracking)
 *       }
 *   }
 *
 * Build with Tracy:
 *   cmake -DENABLE_TRACY=ON ..
 *
 * Build without Tracy (default):
 *   cmake -DENABLE_TRACY=OFF ..  (macros expand to nothing)
 */

#pragma once

#ifdef ENABLE_TRACY
    // Tracy profiler enabled - include full Tracy SDK
    #include <Tracy.hpp>

    #define ZONE_SCOPED ZoneScoped
    #define ZONE_NAMED(name) ZoneScopedN(name)
    #define FRAME_MARK FrameMark
    #define ZONE_VALUE(name, value) TracyPlot(name, value)
#else
    // Tracy disabled - macros expand to nothing (zero overhead)
    #define ZONE_SCOPED
    #define ZONE_NAMED(name)
    #define FRAME_MARK
    #define ZONE_VALUE(name, value)
#endif

