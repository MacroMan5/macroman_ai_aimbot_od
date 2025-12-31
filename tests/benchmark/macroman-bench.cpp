/**
 * @file macroman-bench.cpp
 * @brief CLI benchmark tool for performance regression testing
 *
 * Headless benchmark mode for CI/CD integration. Measures:
 * - Average FPS (frames per second)
 * - P95/P99 latency percentiles
 * - Detection count
 *
 * Returns exit code 0 (pass) or 1 (fail) based on thresholds.
 *
 * Usage:
 *   macroman-bench.exe --frames 500 --inference-delay 6.0 \
 *                      --threshold-avg-fps 120 --threshold-p99-latency 15.0
 *
 * CI/CD Integration:
 *   - Use in GitHub Actions / GitLab CI to fail builds on regression
 *   - Generate performance reports for trending analysis
 */

#include "helpers/FakeScreenCapture.h"
#include "helpers/FakeDetector.h"
#include "core/entities/Detection.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cmath>
#include <string>

using namespace macroman;
using namespace macroman::test;

/**
 * @brief Command-line arguments
 */
struct BenchmarkArgs {
    size_t frameCount{500};                   // Number of frames to process
    uint32_t frameWidth{1920};                // Frame width (pixels)
    uint32_t frameHeight{1080};               // Frame height (pixels)
    float inferenceDelayMs{6.0f};             // Simulated inference delay (ms)
    size_t targetCount{3};                    // Number of targets to detect per frame

    // Performance thresholds (CI/CD pass/fail)
    float thresholdAvgFPS{120.0f};            // Minimum average FPS
    float thresholdP95Latency{12.0f};         // Maximum P95 latency (ms)
    float thresholdP99Latency{15.0f};         // Maximum P99 latency (ms)

    bool verbose{false};                       // Print detailed metrics
};

/**
 * @brief Benchmark results
 */
struct BenchmarkResults {
    size_t framesProcessed{0};
    size_t detectionsTotal{0};

    float avgFPS{0.0f};
    float minLatency{0.0f};
    float maxLatency{0.0f};
    float avgLatency{0.0f};
    float p50Latency{0.0f};
    float p95Latency{0.0f};
    float p99Latency{0.0f};

    std::vector<float> latencySamples;  // All latency measurements (ms)

    bool passed{false};  // Did benchmark meet thresholds?
};

/**
 * @brief Parse command-line arguments
 */
BenchmarkArgs parseArgs(int argc, char* argv[]) {
    BenchmarkArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--frames" && i + 1 < argc) {
            args.frameCount = std::stoul(argv[++i]);
        }
        else if (arg == "--width" && i + 1 < argc) {
            args.frameWidth = std::stoul(argv[++i]);
        }
        else if (arg == "--height" && i + 1 < argc) {
            args.frameHeight = std::stoul(argv[++i]);
        }
        else if (arg == "--inference-delay" && i + 1 < argc) {
            args.inferenceDelayMs = std::stof(argv[++i]);
        }
        else if (arg == "--target-count" && i + 1 < argc) {
            args.targetCount = std::stoul(argv[++i]);
        }
        else if (arg == "--threshold-avg-fps" && i + 1 < argc) {
            args.thresholdAvgFPS = std::stof(argv[++i]);
        }
        else if (arg == "--threshold-p95-latency" && i + 1 < argc) {
            args.thresholdP95Latency = std::stof(argv[++i]);
        }
        else if (arg == "--threshold-p99-latency" && i + 1 < argc) {
            args.thresholdP99Latency = std::stof(argv[++i]);
        }
        else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "MacroMan AI Aimbot - Performance Benchmark Tool\n\n";
            std::cout << "Usage: macroman-bench.exe [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --frames <N>                 Number of frames to process (default: 500)\n";
            std::cout << "  --width <N>                  Frame width in pixels (default: 1920)\n";
            std::cout << "  --height <N>                 Frame height in pixels (default: 1080)\n";
            std::cout << "  --inference-delay <ms>       Simulated inference delay (default: 6.0)\n";
            std::cout << "  --target-count <N>           Number of targets per frame (default: 3)\n";
            std::cout << "  --threshold-avg-fps <fps>    Minimum average FPS (default: 120.0)\n";
            std::cout << "  --threshold-p95-latency <ms> Maximum P95 latency (default: 12.0)\n";
            std::cout << "  --threshold-p99-latency <ms> Maximum P99 latency (default: 15.0)\n";
            std::cout << "  --verbose, -v                Print detailed metrics\n";
            std::cout << "  --help, -h                   Show this help message\n\n";
            std::cout << "Exit codes:\n";
            std::cout << "  0 - All thresholds met (PASS)\n";
            std::cout << "  1 - One or more thresholds failed (FAIL)\n\n";
            std::cout << "Example:\n";
            std::cout << "  macroman-bench.exe --frames 1000 --inference-delay 7.0 \\\n";
            std::cout << "                     --threshold-avg-fps 100 --threshold-p99-latency 20.0\n";
            exit(0);
        }
    }

    return args;
}

/**
 * @brief Calculate percentile from sorted samples
 */
float calculatePercentile(const std::vector<float>& sortedSamples, float percentile) {
    if (sortedSamples.empty()) return 0.0f;

    float index = (percentile / 100.0f) * (sortedSamples.size() - 1);
    size_t lowerIndex = static_cast<size_t>(std::floor(index));
    size_t upperIndex = static_cast<size_t>(std::ceil(index));

    if (lowerIndex == upperIndex) {
        return sortedSamples[lowerIndex];
    }

    // Linear interpolation
    float weight = index - lowerIndex;
    return sortedSamples[lowerIndex] * (1.0f - weight) + sortedSamples[upperIndex] * weight;
}

/**
 * @brief Run benchmark with given configuration
 */
BenchmarkResults runBenchmark(const BenchmarkArgs& args) {
    BenchmarkResults results;

    // Setup FakeScreenCapture
    FakeScreenCapture capture;
    capture.loadSyntheticFrames(args.frameCount, args.frameWidth, args.frameHeight);
    capture.setFrameRate(0);  // No delay (unlimited FPS)

    if (!capture.initialize(nullptr)) {
        std::cerr << "ERROR: Failed to initialize FakeScreenCapture\n";
        return results;
    }

    // Setup FakeDetector
    FakeDetector detector;
    detector.setInferenceDelay(args.inferenceDelayMs);

    if (!detector.initialize("")) {
        std::cerr << "ERROR: Failed to initialize FakeDetector\n";
        return results;
    }

    // Create predefined detections (simulating real targets)
    std::vector<Detection> predefinedDetections;
    for (size_t i = 0; i < args.targetCount; ++i) {
        Detection det;
        det.bbox = {100.0f + i * 150.0f, 100.0f + i * 100.0f, 50.0f, 80.0f};
        det.confidence = 0.9f - i * 0.1f;  // Decreasing confidence
        det.classId = static_cast<int>(i % 3);  // 0=head, 1=chest, 2=body
        det.hitbox = static_cast<HitboxType>(det.classId);
        predefinedDetections.push_back(det);
    }
    detector.loadPredefinedResults(predefinedDetections);

    // Run benchmark
    auto benchmarkStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < args.frameCount; ++i) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Capture frame
        Frame frame = capture.captureFrame();

        // Detect targets (includes simulated inference delay)
        DetectionList detections = detector.detect(frame);

        auto frameEnd = std::chrono::high_resolution_clock::now();

        // Measure latency
        float latencyMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
        results.latencySamples.push_back(latencyMs);

        results.detectionsTotal += detections.size();
        results.framesProcessed++;

        // Progress indicator (every 10%)
        if (args.verbose && i > 0 && i % (args.frameCount / 10) == 0) {
            float progress = (static_cast<float>(i) / args.frameCount) * 100.0f;
            std::cout << "Progress: " << std::fixed << std::setprecision(1) << progress << "%\n";
        }
    }

    auto benchmarkEnd = std::chrono::high_resolution_clock::now();

    // Calculate metrics
    float totalTimeS = std::chrono::duration<float>(benchmarkEnd - benchmarkStart).count();
    results.avgFPS = results.framesProcessed / totalTimeS;

    // Sort latency samples for percentile calculation
    std::vector<float> sortedLatencies = results.latencySamples;
    std::sort(sortedLatencies.begin(), sortedLatencies.end());

    results.minLatency = sortedLatencies.front();
    results.maxLatency = sortedLatencies.back();
    results.avgLatency = std::accumulate(sortedLatencies.begin(), sortedLatencies.end(), 0.0f) / sortedLatencies.size();
    results.p50Latency = calculatePercentile(sortedLatencies, 50.0f);
    results.p95Latency = calculatePercentile(sortedLatencies, 95.0f);
    results.p99Latency = calculatePercentile(sortedLatencies, 99.0f);

    // Check thresholds
    bool fpsPass = results.avgFPS >= args.thresholdAvgFPS;
    bool p95Pass = results.p95Latency <= args.thresholdP95Latency;
    bool p99Pass = results.p99Latency <= args.thresholdP99Latency;
    results.passed = fpsPass && p95Pass && p99Pass;

    return results;
}

/**
 * @brief Print benchmark results
 */
void printResults(const BenchmarkResults& results, const BenchmarkArgs& args) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  MacroMan Benchmark Results\n";
    std::cout << "========================================\n\n";

    // Throughput metrics
    std::cout << "Throughput:\n";
    std::cout << "  Frames Processed:    " << results.framesProcessed << "\n";
    std::cout << "  Detections Total:    " << results.detectionsTotal << "\n";
    std::cout << "  Average FPS:         " << std::fixed << std::setprecision(2) << results.avgFPS;
    if (results.avgFPS >= args.thresholdAvgFPS) {
        std::cout << "  [✓ PASS: >= " << args.thresholdAvgFPS << "]\n";
    } else {
        std::cout << "  [✗ FAIL: < " << args.thresholdAvgFPS << "]\n";
    }

    std::cout << "\nLatency (ms):\n";
    std::cout << "  Min:                 " << std::fixed << std::setprecision(2) << results.minLatency << "\n";
    std::cout << "  Max:                 " << results.maxLatency << "\n";
    std::cout << "  Average:             " << results.avgLatency << "\n";
    std::cout << "  P50 (Median):        " << results.p50Latency << "\n";
    std::cout << "  P95:                 " << results.p95Latency;
    if (results.p95Latency <= args.thresholdP95Latency) {
        std::cout << "  [✓ PASS: <= " << args.thresholdP95Latency << "]\n";
    } else {
        std::cout << "  [✗ FAIL: > " << args.thresholdP95Latency << "]\n";
    }

    std::cout << "  P99:                 " << results.p99Latency;
    if (results.p99Latency <= args.thresholdP99Latency) {
        std::cout << "  [✓ PASS: <= " << args.thresholdP99Latency << "]\n";
    } else {
        std::cout << "  [✗ FAIL: > " << args.thresholdP99Latency << "]\n";
    }

    // Detailed statistics (verbose mode)
    if (args.verbose) {
        std::cout << "\nDetailed Statistics:\n";
        std::cout << "  Detections/Frame:    " << std::fixed << std::setprecision(2);
        std::cout << (static_cast<float>(results.detectionsTotal) / results.framesProcessed) << "\n";
        std::cout << "  Latency Std Dev:     " << std::fixed << std::setprecision(2);

        // Calculate standard deviation
        float mean = results.avgLatency;
        float variance = 0.0f;
        for (float sample : results.latencySamples) {
            variance += (sample - mean) * (sample - mean);
        }
        variance /= results.latencySamples.size();
        float stdDev = std::sqrt(variance);
        std::cout << stdDev << " ms\n";
    }

    // Final verdict
    std::cout << "\n========================================\n";
    if (results.passed) {
        std::cout << "  VERDICT: ✓ PASS (All thresholds met)\n";
    } else {
        std::cout << "  VERDICT: ✗ FAIL (One or more thresholds not met)\n";
    }
    std::cout << "========================================\n\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Parse arguments
    BenchmarkArgs args = parseArgs(argc, argv);

    // Print configuration (verbose mode)
    if (args.verbose) {
        std::cout << "Benchmark Configuration:\n";
        std::cout << "  Frames:              " << args.frameCount << "\n";
        std::cout << "  Resolution:          " << args.frameWidth << "x" << args.frameHeight << "\n";
        std::cout << "  Inference Delay:     " << args.inferenceDelayMs << " ms\n";
        std::cout << "  Target Count:        " << args.targetCount << "\n";
        std::cout << "  FPS Threshold:       >= " << args.thresholdAvgFPS << "\n";
        std::cout << "  P95 Threshold:       <= " << args.thresholdP95Latency << " ms\n";
        std::cout << "  P99 Threshold:       <= " << args.thresholdP99Latency << " ms\n";
        std::cout << "\nRunning benchmark...\n";
    }

    // Run benchmark
    BenchmarkResults results = runBenchmark(args);

    // Print results
    printResults(results, args);

    // Return exit code based on pass/fail
    return results.passed ? 0 : 1;
}
