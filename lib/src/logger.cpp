#include <fstd/logger.h>

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

std::shared_ptr<spdlog::logger> Logger::get_logger() const { return logger_; }

Logger::Logger() {
  try {
    // 1. Create two sinks: color terminal + rotating file sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        get_app_log_dir() / LOG_FILE_NAME, LOG_MAX_SIZE, LOG_MAX_FILES);

    // 2. Set log pattern
    console_sink->set_pattern(LOG_PATTERN);
    file_sink->set_pattern(LOG_PATTERN);

    // 3. Create multi-threaded logger with two sink
    logger_ = std::make_shared<spdlog::logger>(
        "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

    // 4. Set default log level to info (trace will output all levels)
    logger_->set_level(spdlog::level::info);

    logger_->flush_on(spdlog::level::warn);
    spdlog::flush_every(std::chrono::seconds(3));

    // Set default global logger as this instance
    spdlog::set_default_logger(logger_);
  } catch (const spdlog::spdlog_ex &ex) {
    printf("Logger init failed: %s\n", ex.what());
  }
}
