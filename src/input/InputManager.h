#pragma once

#include "core/interfaces/IMouseDriver.h"
#include "core/entities/AimCommand.h"
#include "movement/TrajectoryPlanner.h"
#include "humanization/Humanizer.h"
#include <atomic>
#include <thread>
#include <memory>
#include <chrono>
#include <random>

namespace macroman {

/**
 * @brief Configuration for Input thread behavior
 */
struct InputConfig {
    // Target update rate (nominally 1000Hz, but will vary for humanization)
    int targetHz = 1000;

    // Safety: Stop if no new commands for this duration
    int deadmanThresholdMs = 200;

    // Safety: Emergency shutdown if stale for this duration
    int emergencyShutdownMs = 1000;

    // Humanization: Add timing variance (±20% jitter around target Hz)
    bool enableTimingVariance = true;
    float timingJitterFactor = 0.2f;  // 0.2 = ±20%

    // Screen configuration
    int screenWidth = 1920;
    int screenHeight = 1080;

    // Crosshair position (usually screen center)
    cv::Point2f crosshairPos{960.0f, 540.0f};
};

/**
 * @brief Metrics for Input thread telemetry
 */
struct InputMetrics {
    std::atomic<uint64_t> updateCount{0};
    std::atomic<uint64_t> deadmanTriggered{0};
    std::atomic<uint64_t> movementsExecuted{0};
    std::atomic<float> avgUpdateRate{0.0f};  // Actual Hz
};

/**
 * @class InputManager
 * @brief Orchestrates the 1000Hz input loop with safety mechanisms
 *
 * This is the final stage of the aimbot pipeline. It reads AimCommands
 * from the Tracking thread via atomic variable and translates them to
 * mouse movements using TrajectoryPlanner and IMouseDriver.
 *
 * Safety Features:
 * - Deadman switch: Stops if no new commands for 200ms
 * - Timing variance: Adds ±20% jitter to update rate (800-1200Hz)
 * - Emergency shutdown: Kills thread if stale for >1s
 *
 * Thread Priority: THREAD_PRIORITY_HIGHEST (Windows)
 */
class InputManager {
public:
    /**
     * @brief Construct InputManager with required dependencies
     *
     * @param driver Mouse input driver (Win32Driver or ArduinoDriver)
     * @param planner Trajectory planner for smooth movement
     * @param humanizer Humanization layer (reaction delay, tremor)
     * @param config Input thread configuration
     */
    InputManager(
        std::shared_ptr<IMouseDriver> driver,
        std::shared_ptr<TrajectoryPlanner> planner,
        std::shared_ptr<Humanizer> humanizer,
        const InputConfig& config = {}
    );

    ~InputManager();

    /**
     * @brief Start the 1000Hz input thread
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the input thread gracefully
     */
    void stop();

    /**
     * @brief Check if input thread is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Update the aim command (called by Tracking thread)
     *
     * This stores the command via atomic write. The Input thread
     * reads it via atomic load on every iteration.
     *
     * @param cmd The new aim command
     */
    void updateAimCommand(const AimCommand& cmd);

    /**
     * @brief Get current configuration
     */
    const InputConfig& getConfig() const { return config_; }

    /**
     * @brief Update configuration (thread-safe)
     */
    void setConfig(const InputConfig& config);

    /**
     * @brief Get performance metrics
     */
    const InputMetrics& getMetrics() const { return metrics_; }

private:
    /**
     * @brief Main input loop (runs at 1000Hz with variance)
     *
     * Loop structure:
     * 1. Read AimCommand (atomic load)
     * 2. Check deadman switch (>200ms stale?)
     * 3. Apply Humanizer effects (tremor)
     * 4. Compute trajectory step (TrajectoryPlanner)
     * 5. Send mouse movement (IMouseDriver)
     * 6. Sleep with variance (0.8ms - 1.2ms)
     */
    void inputLoop();

    /**
     * @brief Calculate sleep duration with humanization jitter
     * @param nominalMs Target sleep duration (1.0ms for 1000Hz)
     * @return Actual sleep duration with ±20% jitter
     */
    std::chrono::microseconds calculateSleepDuration(float nominalMs);

    // Dependencies
    std::shared_ptr<IMouseDriver> driver_;
    std::shared_ptr<TrajectoryPlanner> planner_;
    std::shared_ptr<Humanizer> humanizer_;

    // Configuration
    InputConfig config_;

    // Thread state
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> inputThread_;

    // Atomic command from Tracking thread
    std::atomic<AimCommand> latestCommand_{AimCommand{}};

    // Metrics
    InputMetrics metrics_;

    // Timing variance RNG
    std::mt19937 rng_;
    std::uniform_real_distribution<float> jitterDist_;

    // Timing tracking (atomic for thread-safe access from updateAimCommand)
    std::atomic<std::chrono::high_resolution_clock::time_point> lastCommandTime_;
};

} // namespace macroman
