#include "utils/Logger.h"
#include "threading/ThreadManager.h"
#include "threading/LatestFrameQueue.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace macroman;
using namespace std::chrono_literals;

int main() {
    // Initialize logging
    Logger::init("logs/sunone.log", spdlog::level::debug);

    LOG_INFO("========================================");
    LOG_INFO("MacroMan AI Aimbot - Phase 1 Demo");
    LOG_INFO("========================================");

    // Test LatestFrameQueue
    LOG_INFO("Testing LatestFrameQueue...");
    {
        struct TestFrame { int id; };
        LatestFrameQueue<TestFrame> queue;

        queue.push(new TestFrame{1});
        queue.push(new TestFrame{2});
        queue.push(new TestFrame{3});

        auto* frame = queue.pop();
        if (frame && frame->id == 3) {
            LOG_INFO("LatestFrameQueue: Head-drop policy works (got frame 3)");
        } else {
            LOG_ERROR("LatestFrameQueue: Expected frame 3, got frame {}", frame ? std::to_string(frame->id) : "null");
        }
        delete frame;
    }

    // Test ThreadManager
    LOG_INFO("Testing ThreadManager...");
    {
        ThreadManager manager;

        manager.createThread("TestThread", 0, [](std::atomic<bool>& stopFlag) {
            LOG_DEBUG("TestThread started");
            int count = 0;
            while (!stopFlag.load() && count < 5) {
                LOG_DEBUG("TestThread tick {}", count++);
                std::this_thread::sleep_for(100ms);
            }
            LOG_DEBUG("TestThread stopped");
        });

        std::this_thread::sleep_for(300ms);
        LOG_INFO("Stopping threads...");

        if (manager.stopAll()) {
            LOG_INFO("ThreadManager: Graceful shutdown successful");
        } else {
            LOG_ERROR("ThreadManager: Shutdown timeout");
        }
    }

    LOG_INFO("========================================");
    LOG_INFO("Phase 1 Demo Complete");
    LOG_INFO("========================================");

    Logger::shutdown();
    return 0;
}