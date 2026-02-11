#pragma once

#include <cstddef>
#include <cstdint>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace concurrent {

template <typename T>
class IQueue {
 public:
  virtual ~IQueue() {}

  // 返回实现名称，便于故障定位与性能归因。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后指针失效。
  virtual void Release() = 0;

  // 非阻塞入队。
  // 返回：
  // - kOk：入队成功。
  // - kWouldBlock：队列当前不可写（如已满）。
  // 线程安全：由具体实现定义；默认按并发队列语义设计。
  virtual api::Status TryPush(const T& value) = 0;

  // 非阻塞出队。
  // 返回：
  // - kOk：value 为出队元素。
  // - kWouldBlock：队列当前无可读数据。
  // 线程安全：由具体实现定义；默认按并发队列语义设计。
  virtual api::Result<T> TryPop() = 0;

  // 返回队列近似长度。
  // 说明：并发场景下仅用于监控和调优，不保证瞬时强一致。
  virtual std::size_t ApproxSize() const = 0;
};

}  // namespace concurrent
}  // namespace liblogkit
