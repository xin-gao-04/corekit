#pragma once

#include <string>

namespace liblogkit {
namespace log {

enum class LogSeverity { kInfo = 0, kWarning = 1, kError = 2, kFatal = 3 };

struct LoggingOptions {
  std::string log_dir;
  bool session_subdir = true;
  bool simple_format = false;
  bool json_format = false;
  bool async_sink = false;
  int async_queue_size = 8192;
  bool async_drop_when_full = true;
  bool bootstrap_stderr = true;
  bool install_failure_signal_handler = true;
  bool symbolize_stacktrace = true;
  bool glog_file_output = false;
  bool logtostderr = false;
  bool alsologtostderr = false;
  bool colorlogtostderr = true;
  bool log_prefix = true;
  int min_log_level = 0;
  int stderr_threshold = 2;
  int verbosity = 0;
  int max_log_size_mb = 1800;
  int logbufsecs = 30;
  bool stop_logging_if_full_disk = false;
};

}  // namespace log
}  // namespace liblogkit
