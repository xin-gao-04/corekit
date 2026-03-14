// 日志模块示例
//
// 演示 ILogManager 接口的完整使用流程：
//   1. 通过工厂函数创建实例
//   2. Init 初始化（支持配置文件）
//   3. 使用宏写日志（自动附加文件/行号）
//   4. 运行期 Reload 热更新配置
//   5. Shutdown 关闭并释放资源

#include "corekit/corekit.hpp"

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
  const std::string config_path = argc > 1 ? argv[1] : "config/logging.conf";

  // 1. 创建日志管理器
  corekit::log::ILogManager* logger = corekit_create_log_manager();
  if (logger == NULL) {
    std::fprintf(stderr, "corekit_create_log_manager failed\n");
    return 1;
  }

  // 2. 初始化：传入进程名和配置文件路径
  //    配置文件缺失时使用默认配置（仅输出到 stderr）
  corekit::api::Status st = logger->Init(argv[0], config_path);
  if (!st.ok()) {
    std::fprintf(stderr, "Init failed: %s\n", st.message().c_str());
    corekit_destroy_log_manager(logger);
    return 1;
  }

  // 3. 使用宏写日志（推荐）：宏自动附加文件名与行号
  COREKIT_LOG_INFO(logger, "logging demo started");
  COREKIT_LOG_WARNING(logger, "this is a warning");
  COREKIT_LOG_ERROR(logger, "this is an error (non-fatal)");

  // 也可用流式宏拼接动态内容
  int value = 42;
  COREKIT_LOG_INFO_S(logger, "computed value = " << value);

  // 4. 运行期热更新配置（例如切换日志级别）
  st = logger->Reload(config_path);
  if (!st.ok()) {
    COREKIT_LOG_WARNING_S(logger, "Reload failed: " << st.message());
  }

  // 5. 关闭并释放
  logger->Shutdown();
  corekit_destroy_log_manager(logger);
  return 0;
}
