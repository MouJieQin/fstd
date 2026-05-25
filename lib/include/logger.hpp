#pragma once

#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

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
  static Logger &instance() {
    static Logger logger;
    return logger;
  }

  // 对外提供日志器指针（给 spdlog 宏用）
  std::shared_ptr<spdlog::logger> get_logger() const { return logger_; }

  // 禁止拷贝和赋值
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  // 私有的构造函数（单例）
  Logger() {
    try {
      // 1. 创建两个 sink：彩色终端 + 滚动文件
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          LOG_FILE_NAME, LOG_MAX_SIZE, LOG_MAX_FILES);

      // 2. 设置日志格式
      console_sink->set_pattern(LOG_PATTERN);
      file_sink->set_pattern(LOG_PATTERN);

      // 3. 创建多线程安全的日志器（同时绑定两个 sink）
      logger_ = std::make_shared<spdlog::logger>(
          "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

      // 4. 设置默认日志等级（trace 会输出所有等级）
      logger_->set_level(spdlog::level::info);

      logger_->flush_on(spdlog::level::warn);
      spdlog::flush_every(std::chrono::seconds(3));

      spdlog::set_default_logger(logger_); // 设置为全局默认 logger
    } catch (const spdlog::spdlog_ex &ex) {
      printf("Logger init failed: %s\n", ex.what());
    }
  }
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