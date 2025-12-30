#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

namespace macroman {

/**
 * @brief Thread function signature
 * @param stopFlag Reference to atomic bool (checked by thread for shutdown)
 */
using ThreadFunction = std::function<void(std::atomic<bool>& stopFlag)>;

/**
 * @brief Managed thread with priority and lifecycle control
 */
class ManagedThread {
private:
    std::thread thread_;
    std::atomic<bool> stopFlag_{false};
    std::string name_;
    int priority_{0};  // Platform-specific priority value

public:
    ManagedThread(const std::string& name, int priority, ThreadFunction func);
    ~ManagedThread();

    // Non-copyable, non-movable
    ManagedThread(const ManagedThread&) = delete;
    ManagedThread& operator=(const ManagedThread&) = delete;

    /**
     * @brief Signal thread to stop
     */
    void requestStop() noexcept;

    /**
     * @brief Wait for thread to finish (blocking)
     * @param timeout Maximum wait time
     * @return true if joined successfully, false if timeout
     */
    bool join(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief Check if thread is running
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Get thread name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
};

/**
 * @brief Thread manager for pipeline threads
 */
class ThreadManager {
private:
    std::vector<std::unique_ptr<ManagedThread>> threads_;

public:
    ThreadManager() = default;
    ~ThreadManager();

    // Non-copyable, non-movable
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    /**
     * @brief Create and start a managed thread
     * @param name Thread name (for debugging/logging)
     * @param priority Platform priority (Windows: -2 to 2, see SetThreadPriority)
     * @param func Thread function (receives stopFlag reference)
     */
    void createThread(const std::string& name, int priority, ThreadFunction func);

    /**
     * @brief Stop all threads gracefully
     * @param timeout Maximum wait time per thread
     * @return true if all threads stopped cleanly
     */
    bool stopAll(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief Get count of active threads
     */
    [[nodiscard]] size_t getThreadCount() const noexcept { return threads_.size(); }
};

} // namespace macroman
