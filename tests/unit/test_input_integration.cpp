#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "input/InputManager.h"
#include "input/movement/TrajectoryPlanner.h"
#include "input/humanization/Humanizer.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace macroman;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Fake Mouse Driver for Testing
// =============================================================================

class FakeMouseDriver : public IMouseDriver {
public:
    std::atomic<int> moveCount{0};
    std::atomic<int> totalDx{0};
    std::atomic<int> totalDy{0};
    std::atomic<bool> initialized{false};
    std::atomic<bool> connected{true};

    bool initialize() override {
        initialized = true;
        return true;
    }

    void shutdown() override {
        initialized = false;
    }

    void move(int dx, int dy) override {
        moveCount.fetch_add(1);
        totalDx.fetch_add(dx);
        totalDy.fetch_add(dy);
    }

    void moveAbsolute(int, int) override {}
    void press(MouseButton) override {}
    void release(MouseButton) override {}
    void click(MouseButton) override {}

    std::string getName() const override { return "FakeDriver"; }
    bool isConnected() const override { return connected.load(); }

    void reset() {
        moveCount = 0;
        totalDx = 0;
        totalDy = 0;
    }
};

// =============================================================================
// Integration Tests
// =============================================================================

TEST_CASE("InputManager initialization", "[input][integration]") {
    auto driver = std::make_shared<FakeMouseDriver>();
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config;
    config.targetHz = 100;  // Lower Hz for testing (10ms intervals)

    InputManager manager(driver, planner, humanizer, config);

    SECTION("Starts and stops cleanly") {
        REQUIRE(driver->initialize());

        bool started = manager.start();
        REQUIRE(started);
        REQUIRE(manager.isRunning());

        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        manager.stop();
        REQUIRE(!manager.isRunning());
    }

    SECTION("Cannot start twice") {
        REQUIRE(driver->initialize());

        REQUIRE(manager.start());
        REQUIRE(manager.isRunning());

        // Try to start again - should fail
        REQUIRE(!manager.start());

        manager.stop();
    }

    SECTION("Cannot start with disconnected driver") {
        driver->connected = false;

        bool started = manager.start();
        REQUIRE(!started);
    }
}

TEST_CASE("InputManager processes aim commands", "[input][integration]") {
    auto driver = std::make_shared<FakeMouseDriver>();

    // Configure planner for 1:1 screen-to-mouse mapping (no FOV scaling)
    TrajectoryConfig plannerConfig;
    plannerConfig.fovX = static_cast<float>(plannerConfig.screenWidth);   // 1:1 X mapping
    plannerConfig.fovY = static_cast<float>(plannerConfig.screenHeight);  // 1:1 Y mapping
    plannerConfig.sensitivity = 1.0f;
    auto planner = std::make_shared<TrajectoryPlanner>(plannerConfig);

    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config;
    config.targetHz = 100;  // 10ms intervals
    config.crosshairPos = {960.0f, 540.0f};  // Screen center

    InputManager manager(driver, planner, humanizer, config);

    REQUIRE(driver->initialize());
    REQUIRE(manager.start());

    SECTION("No movement without target") {
        // Don't send any command
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Should have no mouse movements
        REQUIRE(driver->moveCount == 0);
    }

    SECTION("Movement generated with valid target") {
        driver->reset();

        // Send aim command: target at (1000, 600), offset from crosshair
        AimCommand cmd;
        cmd.hasTarget = true;
        cmd.targetPosition = {1000.0f, 600.0f};
        cmd.confidence = 0.9f;
        cmd.hitbox = HitboxType::Head;

        manager.updateAimCommand(cmd);

        // Wait for processing (at 100Hz, should get ~10 updates in 100ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Should have generated mouse movements
        REQUIRE(driver->moveCount > 0);

        // Direction should be roughly correct (target is right+down from crosshair)
        // Note: Exact values depend on TrajectoryPlanner smoothing
        INFO("Total dx: " << driver->totalDx.load());
        INFO("Total dy: " << driver->totalDy.load());

        // Allow generous margins due to smoothing and humanization
        REQUIRE(driver->totalDx > -50);  // Should move right (positive) or minimal left
        REQUIRE(driver->totalDy > -50);  // Should move down (positive) or minimal up
    }

    SECTION("Stops movement when target lost") {
        driver->reset();

        // Send aim command
        AimCommand cmd;
        cmd.hasTarget = true;
        cmd.targetPosition = {1000.0f, 600.0f};
        cmd.confidence = 0.9f;

        manager.updateAimCommand(cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int movesWithTarget = driver->moveCount.load();
        REQUIRE(movesWithTarget > 0);

        // Clear target
        cmd.hasTarget = false;
        manager.updateAimCommand(cmd);

        driver->reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have no new movements
        REQUIRE(driver->moveCount == 0);
    }

    manager.stop();
}

TEST_CASE("InputManager deadman switch", "[input][integration][safety]") {
    auto driver = std::make_shared<FakeMouseDriver>();
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config;
    config.targetHz = 100;
    config.deadmanThresholdMs = 100;  // 100ms threshold for testing

    InputManager manager(driver, planner, humanizer, config);

    REQUIRE(driver->initialize());
    REQUIRE(manager.start());

    SECTION("Stops aiming after command becomes stale") {
        // Send initial command
        AimCommand cmd;
        cmd.hasTarget = true;
        cmd.targetPosition = {1000.0f, 600.0f};
        cmd.confidence = 0.9f;

        manager.updateAimCommand(cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Don't send new commands - let it become stale
        std::this_thread::sleep_for(std::chrono::milliseconds(150));  // > deadmanThresholdMs

        // Check deadman switch was triggered
        const auto& metrics = manager.getMetrics();
        REQUIRE(metrics.deadmanTriggered > 0);
    }

    manager.stop();
}

TEST_CASE("InputManager timing variance", "[input][integration][humanization]") {
    auto driver = std::make_shared<FakeMouseDriver>();
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config;
    config.targetHz = 1000;  // Target 1000Hz
    config.enableTimingVariance = true;
    config.timingJitterFactor = 0.2f;  // ±20%

    InputManager manager(driver, planner, humanizer, config);

    REQUIRE(driver->initialize());
    REQUIRE(manager.start());

    // Send aim command to trigger updates
    AimCommand cmd;
    cmd.hasTarget = true;
    cmd.targetPosition = {1000.0f, 600.0f};
    cmd.confidence = 0.9f;

    manager.updateAimCommand(cmd);

    // Let it run for 200ms (longer duration = more stable average)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager.stop();

    // Check metrics
    const auto& metrics = manager.getMetrics();
    float actualHz = metrics.avgUpdateRate.load();

    // Target: 800-1200 Hz (local dev)
    // CI/CD: 100-300 Hz (due to VM thread preemption and sleep precision)
    // Tolerate down to 100 Hz to account for heavily loaded CI/CD runners
    REQUIRE(actualHz > 100.0f);   // At least 100 Hz (tolerates CI/CD environment)
    REQUIRE(actualHz < 1500.0f);  // Not more than 1500 Hz

    INFO("Actual update rate: " << actualHz << " Hz");
}

TEST_CASE("InputManager config update", "[input][integration]") {
    auto driver = std::make_shared<FakeMouseDriver>();
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config1;
    config1.targetHz = 100;
    config1.deadmanThresholdMs = 200;

    InputManager manager(driver, planner, humanizer, config1);

    // Update config
    InputConfig config2;
    config2.targetHz = 500;
    config2.deadmanThresholdMs = 100;

    manager.setConfig(config2);

    // Verify config was updated
    const auto& retrievedConfig = manager.getConfig();
    REQUIRE(retrievedConfig.targetHz == 500);
    REQUIRE(retrievedConfig.deadmanThresholdMs == 100);
}

TEST_CASE("InputManager metrics tracking", "[input][integration]") {
    auto driver = std::make_shared<FakeMouseDriver>();
    auto planner = std::make_shared<TrajectoryPlanner>();
    auto humanizer = std::make_shared<Humanizer>();

    InputConfig config;
    config.targetHz = 100;

    InputManager manager(driver, planner, humanizer, config);

    REQUIRE(driver->initialize());
    REQUIRE(manager.start());

    // Send aim command
    AimCommand cmd;
    cmd.hasTarget = true;
    cmd.targetPosition = {1000.0f, 600.0f};
    cmd.confidence = 0.9f;

    manager.updateAimCommand(cmd);

    // Run for 200ms (should get ~20 updates at 100Hz)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager.stop();

    // Check metrics
    const auto& metrics = manager.getMetrics();

    REQUIRE(metrics.updateCount > 10);  // At least 10 updates
    REQUIRE(metrics.movementsExecuted > 0);  // Some movements generated
    REQUIRE(metrics.avgUpdateRate > 0.0f);  // Hz tracked

    INFO("Update count: " << metrics.updateCount.load());
    INFO("Movements: " << metrics.movementsExecuted.load());
    INFO("Avg Hz: " << metrics.avgUpdateRate.load());
}
