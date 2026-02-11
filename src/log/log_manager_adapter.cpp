#include "log/log_manager_adapter.hpp"

#include "liblogkit/api/version.hpp"
#include "logkit/log_manager.hpp"

namespace liblogkit {
namespace log {
namespace {

logkit::LogSeverity ToLegacySeverity(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kInfo:
      return logkit::LogSeverity::kInfo;
    case LogSeverity::kWarning:
      return logkit::LogSeverity::kWarning;
    case LogSeverity::kError:
      return logkit::LogSeverity::kError;
    case LogSeverity::kFatal:
      return logkit::LogSeverity::kFatal;
    default:
      return logkit::LogSeverity::kInfo;
  }
}

LoggingOptions FromLegacy(const logkit::LoggingOptions& src) {
  LoggingOptions out;
  out.log_dir = src.log_dir;
  out.session_subdir = src.session_subdir;
  out.simple_format = src.simple_format;
  out.json_format = src.json_format;
  out.async_sink = src.async_sink;
  out.async_queue_size = src.async_queue_size;
  out.async_drop_when_full = src.async_drop_when_full;
  out.bootstrap_stderr = src.bootstrap_stderr;
  out.install_failure_signal_handler = src.install_failure_signal_handler;
  out.symbolize_stacktrace = src.symbolize_stacktrace;
  out.glog_file_output = src.glog_file_output;
  out.logtostderr = src.logtostderr;
  out.alsologtostderr = src.alsologtostderr;
  out.colorlogtostderr = src.colorlogtostderr;
  out.log_prefix = src.log_prefix;
  out.min_log_level = src.min_log_level;
  out.stderr_threshold = src.stderr_threshold;
  out.verbosity = src.verbosity;
  out.max_log_size_mb = src.max_log_size_mb;
  out.logbufsecs = src.logbufsecs;
  out.stop_logging_if_full_disk = src.stop_logging_if_full_disk;
  return out;
}

}  // namespace

const char* LogManagerAdapter::Name() const { return "liblogkit.log.glog_adapter"; }

std::uint32_t LogManagerAdapter::ApiVersion() const { return api::kApiVersion; }

void LogManagerAdapter::Release() { delete this; }

api::Status LogManagerAdapter::Init(const std::string& app_name,
                                    const std::string& config_path) {
  if (app_name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "app_name is empty");
  }
  if (!logkit::LogManager::Init(app_name, config_path)) {
    return api::Status(api::StatusCode::kInternalError, "LogManager::Init failed");
  }
  return api::Status::Ok();
}

api::Status LogManagerAdapter::Reload(const std::string& config_path) {
  if (config_path.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "config_path is empty");
  }
  if (!logkit::LogManager::Reload(config_path)) {
    return api::Status(api::StatusCode::kInternalError, "LogManager::Reload failed");
  }
  return api::Status::Ok();
}

api::Status LogManagerAdapter::Log(LogSeverity severity, const std::string& message) {
  logkit::LogManager::Log(ToLegacySeverity(severity), message);
  return api::Status::Ok();
}

api::Result<LoggingOptions> LogManagerAdapter::CurrentOptions() const {
  return api::Result<LoggingOptions>(FromLegacy(logkit::LogManager::CurrentOptions()));
}

api::Status LogManagerAdapter::Shutdown() {
  logkit::LogManager::Shutdown();
  return api::Status::Ok();
}

}  // namespace log
}  // namespace liblogkit
