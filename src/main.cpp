#include "fstd_cli.h"
#include <spdlog/cfg/env.h>

int main(int argc, char **argv) {
  Logger::instance().set_level(spdlog::level::err); // init logger
  spdlog::cfg::load_env_levels();
  return FstdCli(argc, argv).run();
}