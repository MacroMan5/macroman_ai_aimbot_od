#include "utils/Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>

namespace macroman {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init(const std::string& logFilePath, spdlog::level::level_enum level) {
    if (logger_) {
        return;
    }

    // Create logs directory if needed
    std::filesystem::path logPath(logFilePath);
    if (logPath.has_parent_path()) {
        std::filesystem::create_directories(logPath.parent_path());
    }

    // Create sinks
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(level);
    consoleSink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [thread %t] %v");

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath,
        1024 * 1024 * 10,  // 10 MB max file size
        3                   // Keep 3 rotating files
    );
    fileSink->set_level(spdlog::level::trace);
    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v");

    // Create multi-sink logger
    std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink};
    logger_ = std::make_shared<spdlog::logger>("macroman", sinks.begin(), sinks.end());
    logger_->set_level(level);
    logger_->flush_on(spdlog::level::warn);

    // Register as default logger
    spdlog::set_default_logger(logger_);

    LOG_INFO("Logging system initialized");
}

std::shared_ptr<spdlog::logger>& Logger::getInstance() {
    if (!logger_) {
        init();
    }
    return logger_;
}

void Logger::shutdown() {
    if (logger_) {
        LOG_INFO("Shutting down logging system");
        spdlog::shutdown();
        logger_.reset();
    }
}

} // namespace macroman
