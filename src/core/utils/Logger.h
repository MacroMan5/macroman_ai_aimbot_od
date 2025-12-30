#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace macroman {

/**
 * @brief Centralized logging system
 */
class Logger {
public:
    /**
     * @brief Initialize logging system
     * @param logFilePath Path to log file
     * @param level Minimum log level
     */
    static void init(
        const std::string& logFilePath = "logs/macroman.log",
        spdlog::level::level_enum level = spdlog::level::info
    );

    /**
     * @brief Get logger instance
     */
    static std::shared_ptr<spdlog::logger>& getInstance();

    /**
     * @brief Shutdown logging system
     */
    static void shutdown();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace macroman

// Convenience macros
#define LOG_TRACE(...)    ::macroman::Logger::getInstance()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::macroman::Logger::getInstance()->debug(__VA_ARGS__)
#define LOG_INFO(...)     ::macroman::Logger::getInstance()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::macroman::Logger::getInstance()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::macroman::Logger::getInstance()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::macroman::Logger::getInstance()->critical(__VA_ARGS__)
