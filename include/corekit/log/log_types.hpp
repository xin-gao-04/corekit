#pragma once

#include <string>

namespace corekit {
namespace log {

enum class LogSeverity { kInfo = 0, kWarning = 1, kError = 2, kFatal = 3 };

// 日志配置选项。所有字段均有合理默认值，最简用法只需设置 log_dir 即可。
struct LoggingOptions {
  // 日志文件根目录。空字符串表示不写文件（仅输出到 stderr）。
  std::string log_dir;

  // 是否在 log_dir 下自动创建带时间戳的子目录以区分不同运行。
  bool session_subdir = true;

  // 使用简洁纯文本格式输出（"时间 [级别] 消息"）。
  // simple_format 与 json_format 互斥；两者均 false 时使用 glog 默认格式。
  bool simple_format = false;

  // 使用 JSON Lines 格式输出，每行一个 JSON 对象。
  bool json_format = false;

  // 启用异步日志写入，避免 I/O 阻塞业务线程。
  bool async_sink = false;

  // 异步队列最大条目数。超出后行为由 async_drop_when_full 控制。
  int async_queue_size = 8192;

  // true：队列满时丢弃新日志；false：阻塞等待队列有空位。
  bool async_drop_when_full = true;

  // 是否同时向 stderr 输出日志（不影响文件输出）。
  bool logtostderr = false;

  // 最低输出级别：0=Info, 1=Warning, 2=Error, 3=Fatal。
  int min_log_level = 0;

  // 单个日志文件最大大小（MB）。超出后滚动。
  int max_log_size_mb = 1800;

  // 磁盘空间耗尽时停止写日志，避免系统崩溃。
  bool stop_logging_if_full_disk = false;
};

}  // namespace log
}  // namespace corekit

