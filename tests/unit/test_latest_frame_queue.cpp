#include <catch2/catch_test_macros.hpp>
#include "threading/LatestFrameQueue.h"
#include <string>

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
