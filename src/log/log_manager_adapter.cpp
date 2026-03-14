#include "log/log_manager_adapter.hpp"

#include "corekit/api/version.hpp"
#include "log/log_manager_legacy_internal.hpp"

namespace corekit {
namespace log {
namespace {

corekit_legacy::LogSeverity ToLegacySeverity(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kInfo:
      return corekit_legacy::LogSeverity::kInfo;
    case LogSeverity::kWarning:
      return corekit_legacy::LogSeverity::kWarning;
    case LogSeverity::kError:
      return corekit_legacy::LogSeverity::kError;
    case LogSeverity::kFatal:
      return corekit_legacy::LogSeverity::kFatal;
    default:
      return corekit_legacy::LogSeverity::kInfo;
  }
}

LoggingOptions FromLegacy(const corekit_legacy::LoggingOptions& src) {
  LoggingOptions out;
  out.log_dir = src.log_dir;
  out.session_subdir = src.session_subdir;
  out.simple_format = src.simple_format;
  out.json_format = src.json_format;
  out.async_sink = src.async_sink;
  out.async_queue_size = src.async_queue_size;
  out.async_drop_when_full = src.async_drop_when_full;
  out.logtostderr = src.logtostderr;
  out.min_log_level = src.min_log_level;
  out.max_log_size_mb = src.max_log_size_mb;
  out.stop_logging_if_full_disk = src.stop_logging_if_full_disk;
  return out;
}

}  // namespace

const char* LogManagerAdapter::Name() const { return "corekit.log.glog_adapter"; }

std::uint32_t LogManagerAdapter::ApiVersion() const { return api::kApiVersion; }

void LogManagerAdapter::Release() { delete this; }

api::Status LogManagerAdapter::Init(const std::string& app_name,
                                    const std::string& config_path) {
  if (app_name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "app_name is empty");
  }
  if (!corekit_legacy::LogManager::Init(app_name, config_path)) {
    return api::Status(api::StatusCode::kInternalError, "LogManager::Init failed");
  }
  return api::Status::Ok();
}

api::Status LogManagerAdapter::Reload(const std::string& config_path) {
  if (config_path.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "config_path is empty");
  }
  if (!corekit_legacy::LogManager::Reload(config_path)) {
    return api::Status(api::StatusCode::kInternalError, "LogManager::Reload failed");
  }
  return api::Status::Ok();
}

api::Status LogManagerAdapter::Log(LogSeverity severity, const std::string& message) {
  corekit_legacy::LogManager::Log(ToLegacySeverity(severity), message);
  return api::Status::Ok();
}

api::Result<LoggingOptions> LogManagerAdapter::CurrentOptions() const {
  return api::Result<LoggingOptions>(FromLegacy(corekit_legacy::LogManager::CurrentOptions()));
}

api::Status LogManagerAdapter::Shutdown() {
  corekit_legacy::LogManager::Shutdown();
  return api::Status::Ok();
}

}  // namespace log
}  // namespace corekit



