#include "InputManager.h"
#include "core/utils/Logger.h"
#include <Windows.h>  // For SetThreadPriority
#include <sstream>    // For thread ID logging

namespace macroman {

InputManager::InputManager(
    std::shared_ptr<IMouseDriver> driver,
    std::shared_ptr<TrajectoryPlanner> planner,
    std::shared_ptr<Humanizer> humanizer,
    const InputConfig& config)
    : driver_(std::move(driver))
    , planner_(std::move(planner))
    , humanizer_(std::move(humanizer))
    , config_(config)
    , rng_(std::random_device{}())
    , jitterDist_(1.0f - config.timingJitterFactor, 1.0f + config.timingJitterFactor)
{
    // Update crosshair position in trajectory planner config
    auto plannerConfig = planner_->getConfig();
    plannerConfig.screenWidth = config_.screenWidth;
    plannerConfig.screenHeight = config_.screenHeight;
    planner_->setConfig(plannerConfig);
}

InputManager::~InputManager() {
    stop();
}

bool InputManager::start() {
    if (running_) {
        LOG_WARN("InputManager already running");
        return false;
    }

    if (!driver_->isConnected()) {
        LOG_ERROR("Mouse driver not connected - cannot start InputManager");
        return false;
    }

    LOG_INFO("Starting InputManager (target: {}Hz, deadman: {}ms)",
             config_.targetHz, config_.deadmanThresholdMs);

    running_ = true;
    lastCommandTime_ = std::chrono::high_resolution_clock::now();

    inputThread_ = std::make_unique<std::thread>([this]() {
        inputLoop();
    });

    // Set thread priority to HIGHEST (Windows-specific)
#ifdef _WIN32
    HANDLE hThread = static_cast<HANDLE>(inputThread_->native_handle());
    if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
        LOG_WARN("Failed to set Input thread priority to HIGHEST (error: {})", GetLastError());
    } else {
        LOG_INFO("Input thread priority set to HIGHEST");
    }
#endif

    return true;
}

void InputManager::stop() {
    if (!running_) {
        return;
    }

    LOG_INFO("Stopping InputManager...");
    running_ = false;

    if (inputThread_ && inputThread_->joinable()) {
        inputThread_->join();
    }

    LOG_INFO("InputManager stopped (updates: {}, deadman triggers: {}, movements: {})",
             metrics_.updateCount.load(),
             metrics_.deadmanTriggered.load(),
             metrics_.movementsExecuted.load());
}

void InputManager::updateAimCommand(const AimCommand& cmd) {
    latestCommand_.store(cmd, std::memory_order_release);
    lastCommandTime_.store(std::chrono::high_resolution_clock::now(), std::memory_order_release);
}

void InputManager::setConfig(const InputConfig& config) {
    config_ = config;

    // Update jitter distribution
    jitterDist_ = std::uniform_real_distribution<float>(
        1.0f - config.timingJitterFactor,
        1.0f + config.timingJitterFactor
    );

    // Update planner config
    auto plannerConfig = planner_->getConfig();
    plannerConfig.screenWidth = config_.screenWidth;
    plannerConfig.screenHeight = config_.screenHeight;
    planner_->setConfig(plannerConfig);
}

void InputManager::inputLoop() {
    // Convert thread ID to string for logging (std::thread::id not directly formattable)
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    LOG_INFO("Input loop started (thread ID: {})", oss.str());

    using namespace std::chrono;
    const float nominalSleepMs = 1000.0f / config_.targetHz;  // 1.0ms for 1000Hz

    auto loopStartTime = high_resolution_clock::now();
    uint64_t iterationCount = 0;

    while (running_) {
        auto iterationStart = high_resolution_clock::now();

        // ================================================================
        // Step 1: Read latest aim command (atomic load)
        // ================================================================
        AimCommand cmd = latestCommand_.load(std::memory_order_acquire);

        // ================================================================
        // Step 2: Deadman Switch - Check command freshness
        // ================================================================
        auto now = high_resolution_clock::now();
        auto lastCmd = lastCommandTime_.load(std::memory_order_acquire);
        auto staleDuration = duration_cast<milliseconds>(now - lastCmd).count();

        if (staleDuration > config_.deadmanThresholdMs) {
            // Command is stale - disable aiming for safety
            if (cmd.hasTarget) {
                LOG_WARN("Input stale (no new commands for {}ms) - DEADMAN SWITCH ACTIVE", staleDuration);
                metrics_.deadmanTriggered.fetch_add(1, std::memory_order_relaxed);
                cmd.hasTarget = false;  // Override - stop aiming
            }

            // Emergency shutdown if stale for >1s
            if (staleDuration > config_.emergencyShutdownMs) {
                LOG_CRITICAL("Input stale for >{}ms - EMERGENCY SHUTDOWN", config_.emergencyShutdownMs);
                running_ = false;
                break;
            }
        }

        // ================================================================
        // Step 3: Process aim command if valid
        // ================================================================
        if (cmd.hasTarget) {
            // Current position (screen center / crosshair)
            Vec2 current = config_.crosshairPos;
            Vec2 target = Vec2{cmd.targetPosition.x, cmd.targetPosition.y};

            // Apply humanization: Tremor (micro-jitter)
            // dt = time since last iteration (approximately 1ms)
            float dt = nominalSleepMs / 1000.0f;  // Convert to seconds
            target = humanizer_->applyTremor(target, dt);

            // Compute trajectory step (Bezier + 1 Euro filter)
            MouseMovement movement = planner_->planWithPrediction(current, target, cmd.confidence);

            // Execute mouse movement
            if (!movement.isZero()) {
                driver_->move(movement.dx, movement.dy);
                metrics_.movementsExecuted.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            // No target - reset planner state
            planner_->reset();
            humanizer_->resetTremorPhase();
        }

        // ================================================================
        // Step 4: Timing & Metrics
        // ================================================================
        iterationCount++;
        metrics_.updateCount.fetch_add(1, std::memory_order_relaxed);

        // Update average Hz every 100 iterations
        if (iterationCount % 100 == 0) {
            auto elapsed = duration_cast<milliseconds>(now - loopStartTime).count();
            if (elapsed > 0) {
                float actualHz = (iterationCount * 1000.0f) / elapsed;
                metrics_.avgUpdateRate.store(actualHz, std::memory_order_relaxed);
            }
        }

        // ================================================================
        // Step 5: Sleep with humanization jitter
        // ================================================================
        auto sleepDuration = calculateSleepDuration(nominalSleepMs);

        // Account for processing time
        auto iterationEnd = high_resolution_clock::now();
        auto processingTime = duration_cast<microseconds>(iterationEnd - iterationStart);

        if (sleepDuration > processingTime) {
            std::this_thread::sleep_for(sleepDuration - processingTime);
        }
        // else: Processing took longer than target sleep - skip sleep (we're behind)
    }

    // Final metrics calculation (for short-duration tests)
    auto finalElapsed = duration_cast<milliseconds>(high_resolution_clock::now() - loopStartTime).count();
    if (finalElapsed > 0 && iterationCount > 0) {
        float finalHz = (iterationCount * 1000.0f) / finalElapsed;
        metrics_.avgUpdateRate.store(finalHz, std::memory_order_relaxed);
    }

    LOG_INFO("Input loop exited");
}

std::chrono::microseconds InputManager::calculateSleepDuration(float nominalMs) {
    if (!config_.enableTimingVariance) {
        return std::chrono::microseconds(static_cast<int64_t>(nominalMs * 1000));
    }

    // Add ±20% jitter: 1.0ms becomes 0.8ms - 1.2ms
    float jitteredMs = nominalMs * jitterDist_(rng_);

    return std::chrono::microseconds(static_cast<int64_t>(jitteredMs * 1000));
}

} // namespace macroman
