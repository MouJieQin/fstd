#pragma once

#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// 跨平台判断：标准输出是否是终端
#ifdef _WIN32
#include <windows.h>
bool is_terminal() {
  return GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), nullptr) != 0;
}
#else
#include <unistd.h>
inline bool is_terminal() { return isatty(STDOUT_FILENO) == 1; }
#endif

// 日志配置（可自行修改）
#define LOG_FILE_NAME "fstd.log"     // 日志文件名
#define LOG_MAX_SIZE 1024 * 1024 * 5 // 单个日志最大 5MB
#define LOG_MAX_FILES 3              // 最多保留 3 个备份日志
// #define LOG_PATTERN "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %@: %v"  // 日志格式
// #define LOG_PATTERN "[[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v"  //
// 日志格式
#define LOG_PATTERN "[%Y-%m-%d %H:%M:%S] [%^%l%$] [thread %t] [%s:%#] %v"
#define LOG_PATTERN_SHORT "[%Y-%m-%d %H:%M:%S] [%^%l%$] %v"          // 简洁格式
#define LOG_PATTERN_DETAIL "[%Y-%m-%d %H:%M:%S] [%^%l%$] [%s:%#] %v" // 详细格式

class Logger {
public:
  // 获取单例实例
  static Logger &instance();

  // 对外提供日志器指针（给 spdlog 宏用）
  std::shared_ptr<spdlog::logger> get_logger() const;

  // 禁止拷贝和赋值
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  // 私有的构造函数（单例）
  Logger();

private:
  std::shared_ptr<spdlog::logger> logger_;
};

// ==============================================
// 对外使用的极简日志宏（推荐直接用这些！）
// ==============================================
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
