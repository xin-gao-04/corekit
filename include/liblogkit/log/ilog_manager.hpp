#pragma once

#include <string>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"
#include "liblogkit/log/log_types.hpp"

namespace liblogkit {
namespace log {

class ILogManager {
 public:
  virtual ~ILogManager() {}

  // 返回实现名称，便于排查“当前到底绑定了哪个实现”。
  virtual const char* Name() const = 0;

  // 返回实现遵循的 API 版本，用于运行期兼容性检查。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放对象本身。仅对本接口实例有效，调用后指针失效。
  virtual void Release() = 0;

  // 初始化日志系统。
  // 典型时机：进程启动后尽早调用一次。
  // 参数：
  // - app_name: 应用名，常用 argv[0]。
  // - config_path: 配置文件路径，可为空（使用默认配置）。
  // 返回：kOk 表示可开始写日志；失败时 message 给出原因。
  // 线程安全：线程安全；但建议只在主线程调用一次。
  virtual api::Status Init(const std::string& app_name,
                           const std::string& config_path) = 0;

  // 运行期重载配置，不中断业务线程。
  // 典型时机：配置文件修改后由控制面触发。
  // 参数：config_path 必须是可读文件。
  // 返回：kOk 表示重载成功；失败不会破坏既有已生效配置。
  // 线程安全：线程安全。
  virtual api::Status Reload(const std::string& config_path) = 0;

  // 写一条日志，业务代码常用入口。
  // 参数：
  // - severity: 日志级别。
  // - message: 日志正文（UTF-8 文本）。
  // 返回：kOk 表示已交给底层日志系统；失败可降级打印到 stderr。
  // 线程安全：线程安全。
  virtual api::Status Log(LogSeverity severity, const std::string& message) = 0;

  // 读取当前已生效的日志配置快照。
  // 返回：成功时 value 为完整配置。
  // 线程安全：线程安全。
  virtual api::Result<LoggingOptions> CurrentOptions() const = 0;

  // 关闭日志系统并释放底层资源。
  // 典型时机：进程退出前。
  // 返回：kOk 表示关闭完成；重复调用返回 kOk。
  // 线程安全：线程安全，但建议只在退出阶段调用。
  virtual api::Status Shutdown() = 0;
};

}  // namespace log
}  // namespace liblogkit
