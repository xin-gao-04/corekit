#include "liblogkit/liblogkit.hpp"

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
  const std::string config_path = argc > 1 ? argv[1] : "config/logging.conf";

  liblogkit::log::ILogManager* logger = liblogkit_create_log_manager();
  if (logger == NULL) {
    std::fprintf(stderr, "create logger failed\n");
    return 1;
  }

  liblogkit::api::Status st = logger->Init(argv[0], config_path);
  if (!st.ok()) {
    std::fprintf(stderr, "Init failed: %s\n", st.message().c_str());
    liblogkit_destroy_log_manager(logger);
    return 1;
  }

  logger->Log(liblogkit::log::LogSeverity::kInfo,
              "liblogkit interface example started");
  logger->Log(liblogkit::log::LogSeverity::kWarning,
              "This warning is emitted via pure virtual interface.");

  logger->Reload(config_path);
  logger->Shutdown();
  liblogkit_destroy_log_manager(logger);
  return 0;
}
