#include <fstd/logger.h>

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

std::shared_ptr<spdlog::logger> Logger::get_logger() const { return logger_; }

Logger::Logger() {
  try {
    // 1. 创建两个 sink：彩色终端 + 滚动文件
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        get_app_log_dir() / LOG_FILE_NAME, LOG_MAX_SIZE, LOG_MAX_FILES);

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
