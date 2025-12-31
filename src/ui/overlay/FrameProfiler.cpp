#include "FrameProfiler.h"
#include <numeric>
#include <algorithm>

namespace macroman {

void FrameProfiler::update(float captureMs, float detectionMs, float trackingMs, float inputMs) {
    // Store samples in ring buffer
    captureHistory_[writeIndex_] = captureMs;
    detectionHistory_[writeIndex_] = detectionMs;
    trackingHistory_[writeIndex_] = trackingMs;
    inputHistory_[writeIndex_] = inputMs;

    // Advance write index (ring buffer wrap-around)
    writeIndex_ = (writeIndex_ + 1) % HISTORY_SIZE;
    if (writeIndex_ == 0) {
        bufferFilled_ = true;
    }

    // Update cached averages
    avgCapture_ = calculateAverage(captureHistory_);
    avgDetection_ = calculateAverage(detectionHistory_);
    avgTracking_ = calculateAverage(trackingHistory_);
    avgInput_ = calculateAverage(inputHistory_);
}

void FrameProfiler::renderGraphs() {
    ImGui::Text("Frame Profiler (Latency Breakdown)");
    ImGui::Separator();

    // Capture latency graph
    ImGui::Text("Capture: %.2f ms avg (target: <1ms, P95: <2ms)", avgCapture_);
    ImGui::PlotLines("##Capture", captureHistory_.data(), static_cast<int>(HISTORY_SIZE),
                     0, nullptr, 0.0f, 5.0f, ImVec2(0, 60));

    // Detection latency graph
    ImGui::Text("Detection: %.2f ms avg (target: 5-8ms, P95: <10ms)", avgDetection_);
    ImGui::PlotLines("##Detection", detectionHistory_.data(), static_cast<int>(HISTORY_SIZE),
                     0, nullptr, 0.0f, 20.0f, ImVec2(0, 60));

    // Tracking latency graph
    ImGui::Text("Tracking: %.2f ms avg (target: <1ms, P95: <2ms)", avgTracking_);
    ImGui::PlotLines("##Tracking", trackingHistory_.data(), static_cast<int>(HISTORY_SIZE),
                     0, nullptr, 0.0f, 5.0f, ImVec2(0, 60));

    // Input latency graph
    ImGui::Text("Input: %.2f ms avg (target: <0.5ms, P95: <1ms)", avgInput_);
    ImGui::PlotLines("##Input", inputHistory_.data(), static_cast<int>(HISTORY_SIZE),
                     0, nullptr, 0.0f, 2.0f, ImVec2(0, 60));

    // Total end-to-end latency
    float totalAvg = avgCapture_ + avgDetection_ + avgTracking_ + avgInput_;
    ImVec4 totalColor = totalAvg < 10.0f ? ImVec4(0, 1, 0, 1) :   // GREEN if < 10ms
                        totalAvg < 15.0f ? ImVec4(1, 1, 0, 1) :   // YELLOW if 10-15ms
                        ImVec4(1, 0, 0, 1);                       // RED if > 15ms

    ImGui::TextColored(totalColor, "Total End-to-End: %.2f ms (target: <10ms, P95: <15ms)", totalAvg);

    // Bottleneck detection
    std::string bottleneck = detectBottleneck();
    if (!bottleneck.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Bottleneck Detected:");
        ImGui::TextWrapped("%s", bottleneck.c_str());
    }
}

void FrameProfiler::reset() {
    captureHistory_.fill(0.0f);
    detectionHistory_.fill(0.0f);
    trackingHistory_.fill(0.0f);
    inputHistory_.fill(0.0f);
    writeIndex_ = 0;
    bufferFilled_ = false;
    avgCapture_ = 0.0f;
    avgDetection_ = 0.0f;
    avgTracking_ = 0.0f;
    avgInput_ = 0.0f;
}

std::string FrameProfiler::detectBottleneck() const {
    // Thresholds (P95 targets from architecture doc)
    constexpr float CAPTURE_THRESHOLD = 2.0f;     // ms
    constexpr float DETECTION_THRESHOLD = 10.0f;  // ms
    constexpr float TRACKING_THRESHOLD = 2.0f;    // ms
    constexpr float INPUT_THRESHOLD = 1.0f;       // ms

    // Check each stage (return first bottleneck found)
    if (avgDetection_ > DETECTION_THRESHOLD) {
        return "Detection (" + std::to_string(avgDetection_) + "ms)\n"
               "Suggestion: Reduce input size (640x640 -> 416x416) or switch to TensorRT backend.";
    }

    if (avgCapture_ > CAPTURE_THRESHOLD) {
        return "Capture (" + std::to_string(avgCapture_) + "ms)\n"
               "Suggestion: GPU busy or driver lag. Check GPU usage, reduce game graphics settings.";
    }

    if (avgTracking_ > TRACKING_THRESHOLD) {
        return "Tracking (" + std::to_string(avgTracking_) + "ms)\n"
               "Suggestion: Too many targets (>64). Increase confidence threshold or reduce FOV.";
    }

    if (avgInput_ > INPUT_THRESHOLD) {
        return "Input (" + std::to_string(avgInput_) + "ms)\n"
               "Suggestion: Filter complexity too high. Reduce smoothness or disable Bezier curves.";
    }

    return "";  // No bottleneck
}

float FrameProfiler::calculateAverage(const std::array<float, HISTORY_SIZE>& history) const {
    // Calculate average over valid samples
    size_t count = bufferFilled_ ? HISTORY_SIZE : writeIndex_;
    if (count == 0) return 0.0f;

    float sum = std::accumulate(history.begin(), history.begin() + count, 0.0f);
    return sum / static_cast<float>(count);
}

}  // namespace macroman
