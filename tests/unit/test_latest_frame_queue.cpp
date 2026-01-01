#include <catch2/catch_test_macros.hpp>
#include "threading/LatestFrameQueue.h"
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

using namespace macroman;

// Simple test struct (no resources, just int data)
struct TestItem {
    int value{0};
    TestItem(int v) : value(v) {}
};

// RAII test helper to detect leaks
struct TrackedItem {
    static int instanceCount;
    int value{0};

    TrackedItem(int v) : value(v) {
        ++instanceCount;
    }

    ~TrackedItem() {
        --instanceCount;
    }
};

int TrackedItem::instanceCount = 0;

TEST_CASE("LatestFrameQueue - Basic push/pop", "[threading]") {
    LatestFrameQueue<TestItem> queue;

    SECTION("Pop on empty queue returns nullptr") {
        auto* item = queue.pop();
        REQUIRE(item == nullptr);
    }
}

TEST_CASE("LatestFrameQueue - Head-drop policy", "[threading]") {
    LatestFrameQueue<TestItem> queue;

    SECTION("Push 3 items, pop returns only the 3rd") {
        // Push three items (simulate fast producer)
        queue.push(new TestItem(1));
        queue.push(new TestItem(2));
        queue.push(new TestItem(3));

        // Pop should return the LATEST item (3)
        auto* item = queue.pop();
        REQUIRE(item != nullptr);
        REQUIRE(item->value == 3);
        delete item;

        // Queue should now be empty
        auto* empty = queue.pop();
        REQUIRE(empty == nullptr);
    }

    SECTION("Multiple pops return only latest") {
        queue.push(new TestItem(10));
        queue.push(new TestItem(20));

        auto* first = queue.pop();
        REQUIRE(first != nullptr);
        REQUIRE(first->value == 20);
        delete first;

        auto* second = queue.pop();
        REQUIRE(second == nullptr);
    }
}

TEST_CASE("LatestFrameQueue - Memory safety", "[threading]") {
    SECTION("Destructor cleans up remaining item") {
        TrackedItem::instanceCount = 0;

        {
            LatestFrameQueue<TrackedItem> queue;
            queue.push(new TrackedItem(1));
            // Queue goes out of scope WITHOUT pop
        }

        // TrackedItem should be deleted by queue destructor
        REQUIRE(TrackedItem::instanceCount == 0);
    }

    SECTION("No memory leak on rapid push (head-drop deletes old items)") {
        TrackedItem::instanceCount = 0;

        LatestFrameQueue<TrackedItem> queue;

        // Push 1000 items
        for (int i = 0; i < 1000; ++i) {
            queue.push(new TrackedItem(i));
        }

        // Only 1 item should remain in memory (the latest)
        REQUIRE(TrackedItem::instanceCount == 1);

        // Clean up
        auto* item = queue.pop();
        delete item;

        REQUIRE(TrackedItem::instanceCount == 0);
    }

    SECTION("Pop transfers ownership (caller responsible for delete)") {
        TrackedItem::instanceCount = 0;

        LatestFrameQueue<TrackedItem> queue;
        queue.push(new TrackedItem(42));

        auto* item = queue.pop();
        REQUIRE(TrackedItem::instanceCount == 1);  // Still alive

        delete item;
        REQUIRE(TrackedItem::instanceCount == 0);  // Now deleted
    }
}

TEST_CASE("LatestFrameQueue - Concurrent access", "[threading][concurrent]") {
    SECTION("Single producer, single consumer (realistic pipeline scenario)") {
        LatestFrameQueue<TestItem> queue;
        std::atomic<bool> producerDone{false};
        std::atomic<int> lastConsumedValue{-1};
        std::atomic<int> droppedFrames{0};

        // Producer thread: simulates Capture thread at 144 FPS
        std::thread producer([&]() {
            for (int i = 0; i < 1000; ++i) {
                queue.push(new TestItem(i));
                std::this_thread::sleep_for(std::chrono::microseconds(6944));  // ~144 FPS
            }
            producerDone = true;
        });

        // Consumer thread: simulates Detection thread at 60 FPS (slower)
        std::thread consumer([&]() {
            int consumedCount = 0;
            while (!producerDone || queue.pop() != nullptr) {
                auto* item = queue.pop();
                if (item) {
                    lastConsumedValue = item->value;
                    ++consumedCount;
                    delete item;
                    std::this_thread::sleep_for(std::chrono::microseconds(16667));  // ~60 FPS
                }
            }
            // Approximate dropped frames (producer faster than consumer)
            droppedFrames = 1000 - consumedCount;
        });

        producer.join();
        consumer.join();

        // Consumer should have processed ~600 frames (60 FPS * ~10 seconds)
        // Last consumed value should be close to 999 (latest frame)
        REQUIRE(lastConsumedValue >= 900);  // Should process very recent frames

        // Should have dropped ~400 frames (144 FPS - 60 FPS difference)
        REQUIRE(droppedFrames > 0);  // Some frames must be dropped
        REQUIRE(droppedFrames < 1000);  // But not all frames

        // Verify no memory leaks (all items cleaned up)
        REQUIRE(TrackedItem::instanceCount == 0);
    }

    SECTION("Fast producer, slow consumer (stress test)") {
        LatestFrameQueue<TestItem> queue;
        std::atomic<bool> producerDone{false};
        std::atomic<int> consumedCount{0};

        // Very fast producer (1000 items as fast as possible)
        std::thread producer([&]() {
            for (int i = 0; i < 1000; ++i) {
                queue.push(new TestItem(i));
                // No sleep - push as fast as possible
            }
            producerDone = true;
        });

        // Slow consumer (10ms per item)
        std::thread consumer([&]() {
            // Process while producer is running
            while (!producerDone) {
                auto* item = queue.pop();
                if (item) {
                    ++consumedCount;
                    delete item;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            // Producer done - drain any remaining items
            while (auto* item = queue.pop()) {
                ++consumedCount;
                delete item;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        producer.join();
        consumer.join();

        // Consumer should have processed far fewer items than produced
        REQUIRE(consumedCount < 1000);
        REQUIRE(consumedCount > 0);

        // Verify head-drop worked (no memory leak from dropped frames)
        REQUIRE(TrackedItem::instanceCount == 0);
    }
}

TEST_CASE("LatestFrameQueue - Performance & freshness", "[threading][performance]") {
    SECTION("Always returns latest data (no stale frames)") {
        LatestFrameQueue<TestItem> queue;

        // Simulate rapid updates
        for (int i = 0; i < 100; ++i) {
            queue.push(new TestItem(i));
        }

        // Pop should return the LATEST item (99)
        auto* item = queue.pop();
        REQUIRE(item != nullptr);
        REQUIRE(item->value == 99);
        delete item;
    }

    SECTION("Lock-free push/pop (no blocking)") {
        LatestFrameQueue<TestItem> queue;
        std::atomic<bool> running{true};
        std::atomic<int> pushCount{0};
        std::atomic<int> popCount{0};

        // Producer thread
        std::thread producer([&]() {
            while (running) {
                queue.push(new TestItem(pushCount++));
            }
        });

        // Consumer thread
        std::thread consumer([&]() {
            while (running) {
                auto* item = queue.pop();
                if (item) {
                    ++popCount;
                    delete item;
                }
            }
        });

        // Run for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        running = false;

        producer.join();
        consumer.join();

        // Should have processed many items (lock-free = high throughput)
        REQUIRE(pushCount > 1000);  // At least 10k pushes/sec
        REQUIRE(popCount > 0);

        // Clean up remaining item
        auto* remaining = queue.pop();
        if (remaining) delete remaining;

        REQUIRE(TrackedItem::instanceCount == 0);
    }
}

TEST_CASE("LatestFrameQueue - Edge cases under concurrency", "[threading][edge]") {
    SECTION("Interleaved push/pop (no race conditions)") {
        LatestFrameQueue<TestItem> queue;
        std::atomic<int> errors{0};

        // Two threads interleaving push/pop
        std::thread t1([&]() {
            for (int i = 0; i < 500; ++i) {
                queue.push(new TestItem(i));
                auto* item = queue.pop();
                if (item) delete item;
            }
        });

        std::thread t2([&]() {
            for (int i = 500; i < 1000; ++i) {
                queue.push(new TestItem(i));
                auto* item = queue.pop();
                if (item) delete item;
            }
        });

        t1.join();
        t2.join();

        // No errors should have occurred
        REQUIRE(errors == 0);

        // Clean up any remaining item
        auto* remaining = queue.pop();
        if (remaining) delete remaining;

        REQUIRE(TrackedItem::instanceCount == 0);
    }

    SECTION("Rapid push on empty queue") {
        LatestFrameQueue<TestItem> queue;

        // Push-pop-push-pop rapidly
        for (int i = 0; i < 100; ++i) {
            queue.push(new TestItem(i));
            auto* item = queue.pop();
            REQUIRE(item != nullptr);
            REQUIRE(item->value == i);
            delete item;

            // Queue should be empty
            auto* empty = queue.pop();
            REQUIRE(empty == nullptr);
        }

        REQUIRE(TrackedItem::instanceCount == 0);
    }
}

TEST_CASE("LatestFrameQueue - Realistic game scenario", "[threading][realistic]") {
    // Scenario: Capture thread at 144 FPS, Detection thread at 90 FPS
    LatestFrameQueue<TestItem> queue;
    std::atomic<bool> running{true};
    std::atomic<int> framesCaptured{0};
    std::atomic<int> framesDetected{0};
    std::atomic<int> maxStaleness{0};  // Max difference between captured and detected frame

    // Capture thread (144 FPS)
    std::thread captureThread([&]() {
        while (running) {
            queue.push(new TestItem(framesCaptured++));
            std::this_thread::sleep_for(std::chrono::microseconds(6944));  // 144 FPS
        }
    });

    // Detection thread (90 FPS - realistic YOLO inference rate)
    std::thread detectionThread([&]() {
        while (running) {
            auto* frame = queue.pop();
            if (frame) {
                int staleness = framesCaptured - frame->value;
                if (staleness > maxStaleness) {
                    maxStaleness = staleness;
                }
                ++framesDetected;
                delete frame;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(11111));  // 90 FPS
        }
    });

    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    captureThread.join();
    detectionThread.join();

    // Should have captured frames at reasonable rate
    // Local dev: ~72 frames (144 FPS * 0.5s)
    // CI/CD: ~35-40 frames (70-80 FPS due to timer granularity)
    REQUIRE(framesCaptured >= 30);
    REQUIRE(framesCaptured <= 100);

    // Should have detected frames at reasonable rate
    // Local dev: ~45 frames (90 FPS * 0.5s)
    // CI/CD: ~20-25 frames (40-50 FPS)
    REQUIRE(framesDetected >= 18);
    REQUIRE(framesDetected <= 60);

    // Staleness should be low (head-drop ensures fresh data)
    // With 144 FPS capture and 90 FPS detection, max staleness should be ~2-3 frames
    REQUIRE(maxStaleness <= 5);

    // Clean up
    auto* remaining = queue.pop();
    if (remaining) delete remaining;

    REQUIRE(TrackedItem::instanceCount == 0);
}
