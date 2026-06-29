//
//  Copyright (c) 2026 Moujie Qin. All rights reserved.
//  MIT License
//
#pragma once

#include <cstdlib>
#include <filesystem>
#include <memory>
#ifdef _WIN32
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#endif
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// Cross-platform check: standard output is terminal
#ifdef _WIN32
#include <windows.h>
bool is_terminal() {
  return GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), nullptr) != 0;
}
#else
#include <unistd.h>
inline bool is_terminal() { return isatty(STDOUT_FILENO) == 1; }
#endif

// Get application log directory: priority system standard path, fallback to
// ./logs
inline std::filesystem::path
get_app_log_dir(const std::string &app_name = "Fstd") {
  std::filesystem::path log_dir;

#ifdef _WIN32
  // Windows: %LOCALAPPDATA%\app_name\Logs
  const char *local_appdata = std::getenv("LOCALAPPDATA");
  if (local_appdata) {
    log_dir = std::filesystem::path(local_appdata) / app_name / "Logs";
  }
#elif __APPLE__
  // macOS: ~/Library/Logs/app_name
  const char *home = std::getenv("HOME");
  if (home) {
    log_dir = std::filesystem::path(home) / "Library" / "Logs" / app_name;
  }
#elif __linux__
  // Linux: ~/.local/share/app_name/logs
  const char *home = std::getenv("HOME");
  if (home) {
    log_dir =
        std::filesystem::path(home) / ".local" / "share" / app_name / "logs";
  }
#endif
  // Fallback: logs directory in current directory
  if (log_dir.empty()) { log_dir = std::filesystem::current_path() / "logs"; }

  // Create log directory (including parent directories)
  try {
    std::filesystem::create_directories(log_dir);
  } catch (const std::filesystem::filesystem_error &e) {
    printf("Failed to create log dir: %s\n", e.what());
  }

  return log_dir;
}

#define LOG_FILE_NAME "fstd.log"     // Log file name
#define LOG_MAX_SIZE 1024 * 1024 * 5 // Max log file size: 5MB
#define LOG_MAX_FILES 3              // Keep 3 backup files
#define LOG_PATTERN "[%Y-%m-%d %H:%M:%S] [%^%l%$] [thread %t] [%s:%#] %v"

class Logger {
public:
  static Logger &instance();

  std::shared_ptr<spdlog::logger> get_logger() const;

  void set_level(spdlog::level::level_enum l);

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  Logger();

private:
  std::shared_ptr<spdlog::logger> logger_;
};

#define LOG_TRACE(...)                                                         \
  SPDLOG_LOGGER_TRACE(Logger::instance().get_logger(), __VA_ARGS__)
#define LOG_DEBUG(...)                                                         \
  SPDLOG_LOGGER_DEBUG(Logger::instance().get_logger(), __VA_ARGS__)
#define LOG_INFO(...)                                                          \
  SPDLOG_LOGGER_INFO(Logger::instance().get_logger(), __VA_ARGS__)
#define LOG_WARN(...)                                                          \
  SPDLOG_LOGGER_WARN(Logger::instance().get_logger(), __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  SPDLOG_LOGGER_ERROR(Logger::instance().get_logger(), __VA_ARGS__)
#define LOG_CRITICAL(...)                                                      \
  SPDLOG_LOGGER_CRITICAL(Logger::instance().get_logger(), __VA_ARGS__)
