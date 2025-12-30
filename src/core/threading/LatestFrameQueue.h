#pragma once

#include <atomic>
#include <memory>

namespace macroman {

/**
 * @brief Lock-free single-slot queue for real-time frame pipeline.
 * Always returns the LATEST pushed item, discarding intermediate frames.
 * 
 * DESIGN:
 * - Single atomic pointer slot (8 bytes, naturally aligned)
 * - std::atomic::exchange() for push (memory_order_release)
 * - std::atomic::exchange() for pop (memory_order_acquire)
 * - No mutexes, no condition variables, no syscalls
 * 
 * OWNERSHIP:
 * - Queue takes ownership on push.
 * - Old frame (replaced by push) is DELETED by queue.
 * - Caller takes ownership on pop (caller must delete).
 * 
 * PERFORMANCE TARGET: <1us for both push and pop.
 */
template<typename T>
class LatestFrameQueue {
private:
    // Single slot for latest item
    std::atomic<T*> slot{nullptr};

    // Verify lock-free guarantee at compile time
    static_assert(std::atomic<T*>::is_always_lock_free,
                  "LatestFrameQueue requires lock-free atomic pointers");

public:
    LatestFrameQueue() = default;

    // Non-copyable, non-movable (contains atomic)
    LatestFrameQueue(const LatestFrameQueue&) = delete;
    LatestFrameQueue& operator=(const LatestFrameQueue&) = delete;
    LatestFrameQueue(LatestFrameQueue&&) = delete;
    LatestFrameQueue& operator=(LatestFrameQueue&&) = delete;

    ~LatestFrameQueue() {
        // Clean up any remaining item
        T* remaining = slot.exchange(nullptr, std::memory_order_acquire);
        delete remaining;
    }

    /**
     * @brief Push new item (takes ownership)
     * @param newItem Pointer to heap-allocated item (caller transfers ownership)
     */
    void push(T* newItem) {
        // Exchange: atomically swap newItem into slot, get old value
        T* oldItem = slot.exchange(newItem, std::memory_order_release);

        // Delete old frame (head-drop policy)
        if (oldItem != nullptr) {
            delete oldItem;
        }
    }

    /**
     * @brief Pop latest item (transfers ownership to caller)
     * @return Pointer to item, or nullptr if queue empty
     */
    [[nodiscard]] T* pop() {
        // Exchange: atomically swap nullptr into slot, get current value
        return slot.exchange(nullptr, std::memory_order_acquire);
    }

    /**
     * @brief Check if queue has an item (non-blocking)
     * 
     * WARNING: Result may be stale immediately after return.
     * Only use for debugging/metrics, not for control flow.
     */
    [[nodiscard]] bool hasItem() const noexcept {
        return slot.load(std::memory_order_relaxed) != nullptr;
    }
};

} // namespace macroman
