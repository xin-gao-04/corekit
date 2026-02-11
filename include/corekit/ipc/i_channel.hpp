#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "corekit/api/status.hpp"

namespace corekit {
namespace ipc {

struct ChannelOptions {
  // 定义ChannelOptions结构体的成员变量
  std::string name;         // 通道唯一名，建议业务自定义前缀
  std::uint32_t capacity = 1024;   // 环形队列槽位数，必须 > 0
  std::uint32_t message_max_bytes = 4096;   // 消息最大字节数
  bool drop_when_full = true;    // 当缓冲区满时是否丢弃消息
  std::uint32_t timeout_ms = 0;     // 等待超时时间（毫秒）
};

struct ChannelStats {
  // 定义ChannelStats结构体的成员变量
  std::uint64_t send_ok = 0;    // 发送成功次数
  std::uint64_t recv_ok = 0;    // 接收成功次数
  std::uint64_t dropped_when_full = 0;   // 当缓冲区满时丢弃消息次数
  std::uint64_t would_block_send = 0;   // 发送时会阻塞的次数
  std::uint64_t would_block_recv = 0;   // 接收时会阻塞的次数
};

class IChannel {
 public:
  virtual ~IChannel() {}

  // 返回实现名称，用于日志与故障排查。
  virtual const char* Name() const = 0;

  // 返回 API 版本，便于运行期检测动态库兼容性。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象不可再使用。
  virtual void Release() = 0;

  // 以“服务端”角色创建通道并初始化共享资源。
  // 参数：
  // - options.name: 通道唯一名，建议业务自定义前缀。
  // - options.capacity: 环形队列槽位数，必须 > 0。
  // - options.message_max_bytes: 单消息最大字节数。
  // 返回：kOk 表示创建成功；kAlreadyInitialized 表示已打开通道。
  // 线程安全：仅在初始化阶段调用一次。
  virtual api::Status OpenServer(const ChannelOptions& options) = 0;

  // 以“客户端”角色连接已存在通道。
  // 参数：options.name 必须与服务端一致。
  // 返回：kOk 表示连接成功；kNotFound 表示服务端尚未创建。
  // 线程安全：仅在初始化阶段调用一次。
  virtual api::Status OpenClient(const ChannelOptions& options) = 0;

  // 关闭通道并释放本进程侧句柄。
  // 返回：kOk 表示关闭完成；重复调用允许返回 kOk。
  // 线程安全：线程安全。
  virtual api::Status Close() = 0;

  // 非阻塞发送一条二进制消息。
  // 参数：
  // - data: 消息数据首地址。
  // - size: 消息字节数，必须 <= message_max_bytes。
  // 行为：
  // - 队列满且 drop_when_full=true: 返回 kWouldBlock，并计入 dropped 统计。
  // - 队列满且 drop_when_full=false: 当前版本同样返回 kWouldBlock（不阻塞业务线程）。
  // 返回：kOk 表示发送成功。
  // 线程安全：单发送线程模型；多发送线程请在外部加锁。
  virtual api::Status TrySend(const void* data, std::uint32_t size) = 0;

  // 非阻塞接收一条消息。
  // 参数：
  // - buffer: 用户提供的接收缓存。
  // - buffer_size: 缓存大小。
  // 返回：
  // - kOk: value 为实际拷贝字节数。
  // - kWouldBlock: 当前无可读消息。
  // - kBufferTooSmall: 缓冲不足，message 会给出需要的最小字节数。
  // 线程安全：单接收线程模型；多接收线程请在外部加锁。
  virtual api::Result<std::uint32_t> TryRecv(void* buffer,
                                             std::uint32_t buffer_size) = 0;

  // 获取累计统计（发送、接收、丢弃、would-block 计数）。
  // 用途：做背压监控和运行态观测。
  // 线程安全：线程安全。
  virtual ChannelStats GetStats() const = 0;
};

}  // namespace ipc
}  // namespace corekit

