#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"

namespace corekit {
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

  // 非阻塞移动入队，避免不必要的拷贝。
  // 返回语义与 TryPush(const T&) 一致。
  virtual api::Status TryPushMove(T&& value) = 0;

  // 批量非阻塞入队。
  // 参数：
  // - values: 输入数组首地址。
  // - count: 输入元素数量。
  // - pushed: 实际入队数量（可为 nullptr）。
  // 返回：
  // - kOk：全部入队成功。
  // - kWouldBlock：部分或全部元素未能写入（通常为容量限制）。
  // - kInvalidArgument：values 为 nullptr 且 count > 0。
  virtual api::Status TryPushBatch(const T* values, std::size_t count, std::size_t* pushed) = 0;

  // 非阻塞出队。
  // 返回：
  // - kOk：value 为出队元素。
  // - kWouldBlock：队列当前无可读数据。
  // 线程安全：由具体实现定义；默认按并发队列语义设计。
  virtual api::Result<T> TryPop() = 0;

  // 返回队列近似长度。
  // 说明：并发场景下仅用于监控和调优，不保证瞬时强一致。
  virtual std::size_t ApproxSize() const = 0;

  // 返回当前是否为空（近似语义，适合快速分支判断）。
  virtual bool IsEmpty() const = 0;

  // 非阻塞窥视队首元素，不移除数据。
  // 返回：
  // - kOk：*out 为当前队首。
  // - kWouldBlock：队列为空。
  // - kInvalidArgument：out 为 nullptr。
  virtual api::Status TryPeek(T* out) const = 0;

  // 批量非阻塞出队。
  // 参数：
  // - out_values: 输出数组首地址。
  // - capacity: 输出缓冲可写元素数。
  // - popped: 实际出队数量（可为 nullptr）。
  // 返回：
  // - kOk：至少成功出队 1 个元素。
  // - kWouldBlock：当前没有可读数据。
  // - kInvalidArgument：out_values 为 nullptr 且 capacity > 0。
  virtual api::Status TryPopBatch(T* out_values, std::size_t capacity, std::size_t* popped) = 0;

  // 清空队列中的当前元素。
  virtual api::Status Clear() = 0;

  // 返回容量上限。0 表示“无固定上限/由实现决定”。
  virtual std::size_t Capacity() const = 0;
};

}  // namespace concurrent
}  // namespace corekit

