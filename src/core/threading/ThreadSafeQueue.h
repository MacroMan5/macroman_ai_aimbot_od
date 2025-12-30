#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace sunone {

template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t maxSize = 0) : maxSize_(maxSize) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (maxSize_ > 0) {
            // If full, drop oldest
            while (queue_.size() >= maxSize_) {
                queue_.pop();
            }
        }

        queue_.push(std::move(item));
        lock.unlock();
        cv_.notify_one();
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    std::optional<T> waitPop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    size_t maxSize_;
};

} // namespace sunone
