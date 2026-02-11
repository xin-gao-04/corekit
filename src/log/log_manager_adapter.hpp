#pragma once

#include "liblogkit/log/ilog_manager.hpp"

namespace liblogkit {
namespace log {

class LogManagerAdapter : public ILogManager {
 public:
  LogManagerAdapter() {}
  ~LogManagerAdapter() override {}

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status Init(const std::string& app_name, const std::string& config_path) override;
  api::Status Reload(const std::string& config_path) override;
  api::Status Log(LogSeverity severity, const std::string& message) override;
  api::Result<LoggingOptions> CurrentOptions() const override;
  api::Status Shutdown() override;
};

}  // namespace log
}  // namespace liblogkit
