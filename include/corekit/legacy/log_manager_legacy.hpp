#pragma once

#include <string>

#if defined(_WIN32)
#if defined(COREKIT_BUILD_DLL)
#define COREKIT_API __declspec(dllexport)
#elif defined(COREKIT_USE_DLL)
#define COREKIT_API __declspec(dllimport)
#else
#define COREKIT_API
#endif
#else
#define COREKIT_API
#endif

namespace corekit_legacy {

enum class LogSeverity { kInfo = 0, kWarning = 1, kError = 2, kFatal = 3 };

// Normalized logging options parsed from a config file and applied to glog flags.
struct LoggingOptions {
  std::string log_dir;
  bool session_subdir = true;  // create <log_dir>/<timestamp>/ for this run
  bool simple_format = false;  // use custom sink: "[I] message"
  bool json_format = false;    // use custom sink: JSON lines
  bool async_sink = false;     // write custom sink asynchronously
  int async_queue_size = 8192;
  bool async_drop_when_full = true;
  // Bootstrap to stderr before full options are applied. Helps avoid early file-open failures.
  bool bootstrap_stderr = true;
  // Crash diagnostics: installs glog failure signal handler once per process.
  bool install_failure_signal_handler = true;
  // Enable symbolized stacktrace output where glog/toolchain supports it.
  bool symbolize_stacktrace = true;
  // When false, disable glog per-severity file output and keep only custom sink files.
  bool glog_file_output = false;
  bool logtostderr = false;
  bool alsologtostderr = false;
  bool colorlogtostderr = true;
  bool log_prefix = true;
  int min_log_level = 0;       // INFO=0, WARNING=1, ERROR=2, FATAL=3
  int stderr_threshold = 2;    // glog treats this as ERROR by default
  int verbosity = 0;           // VLOG level
  int max_log_size_mb = 1800;  // glog default is 1800 MB
  int logbufsecs = 30;
  bool stop_logging_if_full_disk = false;
};

class COREKIT_API LogManager {
 public:
  // Initialize glog with an application name and optional config file.
  // Safe to call once at process startup.
  static bool Init(const std::string& app_name,
                   const std::string& config_path = {});

  // Reload configuration at runtime. Returns false on load/apply failures.
  static bool Reload(const std::string& config_path);

  // Access the currently applied options.
  static LoggingOptions CurrentOptions();

  // Shutdown glog. Call once during program teardown.
  static void Shutdown();

  // Lightweight logging API that avoids exposing glog headers to callers.
  static void Log(LogSeverity severity, const std::string& message);

 private:
  static bool ApplyOptions(const LoggingOptions& options);
  static LoggingOptions LoadFromFile(const std::string& path, bool* ok);
};

}  // namespace corekit_legacy


