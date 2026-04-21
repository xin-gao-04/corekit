#pragma once

#include "corekit/log/ilog_manager.hpp"

namespace corekit {
namespace log {

// glog 日志管理器，直接实现 ILogManager 接口。
// 通过工厂函数 corekit_create_log_manager() 创建，无需外部感知此类。
class GlogLogManager : public ILogManager {
 public:
  GlogLogManager() {}
  ~GlogLogManager() override {}

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status Init(const std::string& app_name, const std::string& config_path) override;
  api::Status Reload(const std::string& config_path) override;
  api::Status Log(LogSeverity severity, const std::string& message) override;
  api::Status LogWithSource(LogSeverity severity, const std::string& message,
                            const char* file, int line) override;
  api::Result<LoggingOptions> CurrentOptions() const override;
  api::Status Shutdown() override;
};

}  // namespace log
}  // namespace corekit
