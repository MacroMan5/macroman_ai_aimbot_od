#include "threading/ThreadManager.h"
#include <iostream>
#include <future>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace macroman {

ManagedThread::ManagedThread(const std::string& name, int priority, ThreadFunction func)
    : name_(name), priority_(priority)
{
    // Launch thread
    thread_ = std::thread([this, func]() {
        // Set thread priority (Windows-specific)
#ifdef _WIN32
        HANDLE handle = GetCurrentThread();

        // Map priority: -2=IDLE, -1=BELOW_NORMAL, 0=NORMAL, 1=ABOVE_NORMAL, 2=HIGHEST
        int winPriority = THREAD_PRIORITY_NORMAL;
        switch (priority_) {
            case -2: winPriority = THREAD_PRIORITY_IDLE; break;
            case -1: winPriority = THREAD_PRIORITY_BELOW_NORMAL; break;
            case 0:  winPriority = THREAD_PRIORITY_NORMAL; break;
            case 1:  winPriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
            case 2:  winPriority = THREAD_PRIORITY_HIGHEST; break;
            case 3:  winPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
        }

        if (!SetThreadPriority(handle, winPriority)) {
            std::cerr << "[ThreadManager] Warning: Failed to set priority for thread '"
                      << name_ << "'" << std::endl;
        }
#endif

        // Run thread function (passes stopFlag by reference)
        func(stopFlag_);
    });
}

ManagedThread::~ManagedThread() {
    if (thread_.joinable()) {
        requestStop();
        thread_.join();
    }
}

void ManagedThread::requestStop() noexcept {
    stopFlag_.store(true, std::memory_order_release);
}

bool ManagedThread::join(std::chrono::milliseconds timeout) {
    if (!thread_.joinable()) {
        return true;
    }

    // Since std::thread doesn't have join_for, we use a simple busy-wait or std::async
    // std::async is safer for timeout simulation
    auto future = std::async(std::launch::async, [this]() {
        thread_.join();
    });

    if (future.wait_for(timeout) == std::future_status::timeout) {
        std::cerr << "[ThreadManager] Error: Thread '" << name_
                  << "' did not stop within " << timeout.count() << "ms" << std::endl;
        return false;
    }

    return true;
}

bool ManagedThread::isRunning() const noexcept {
    return thread_.joinable() && !stopFlag_.load(std::memory_order_acquire);
}

bool ManagedThread::setCoreAffinity(int coreId) noexcept {
#ifdef _WIN32
    if (!thread_.joinable()) {
        std::cerr << "[ThreadManager] Error: Cannot set affinity on non-running thread '"
                  << name_ << "'" << std::endl;
        return false;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(thread_.native_handle());
    DWORD_PTR mask = 1ULL << coreId;

    if (SetThreadAffinityMask(handle, mask) == 0) {
        std::cerr << "[ThreadManager] Warning: Failed to set affinity for thread '"
                  << name_ << "' to core " << coreId << std::endl;
        return false;
    }

    return true;
#else
    // Non-Windows platforms not supported (could add pthread_setaffinity_np for Linux)
    (void)coreId;  // Suppress unused parameter warning
    std::cerr << "[ThreadManager] Warning: Thread affinity not supported on this platform" << std::endl;
    return false;
#endif
}

// ============================================================================
// ThreadManager Implementation
// ============================================================================

ThreadManager::~ThreadManager() {
    stopAll();
}

void ThreadManager::createThread(const std::string& name, int priority, ThreadFunction func) {
    threads_.emplace_back(std::make_unique<ManagedThread>(name, priority, func));
}

bool ThreadManager::stopAll(std::chrono::milliseconds timeout) {
    // Signal all threads to stop
    for (auto& thread : threads_) {
        thread->requestStop();
    }

    // Join with timeout
    bool allStopped = true;
    for (auto& thread : threads_) {
        if (!thread->join(timeout)) {
            allStopped = false;
        }
    }

    threads_.clear();
    return allStopped;
}

bool ThreadManager::setCoreAffinity(size_t threadIndex, int coreId) {
    if (threadIndex >= threads_.size()) {
        std::cerr << "[ThreadManager] Error: Thread index " << threadIndex
                  << " out of range (count: " << threads_.size() << ")" << std::endl;
        return false;
    }

    // Skip affinity on systems with <6 cores (not beneficial, per design doc)
    unsigned int coreCount = getCPUCoreCount();
    if (coreCount < 6) {
        std::cerr << "[ThreadManager] Info: Skipping thread affinity on " << coreCount
                  << "-core system (only beneficial on 6+ cores)" << std::endl;
        return false;
    }

    // Validate core ID
    if (coreId < 0 || static_cast<unsigned int>(coreId) >= coreCount) {
        std::cerr << "[ThreadManager] Error: Core ID " << coreId
                  << " out of range (0-" << (coreCount - 1) << ")" << std::endl;
        return false;
    }

    return threads_[threadIndex]->setCoreAffinity(coreId);
}

unsigned int ThreadManager::getCPUCoreCount() noexcept {
    unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? count : 1;  // Fallback to 1 if detection fails
}

} // namespace macroman
