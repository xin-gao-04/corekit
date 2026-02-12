#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"

namespace corekit {
namespace concurrent {

template <typename T>
class IRingBuffer {
 public:
  virtual ~IRingBuffer() {}

  // 返回实现名称。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 非阻塞写入一个元素。
  // 返回：
  // - kOk：写入成功。
  // - kWouldBlock：缓冲区已满。
  virtual api::Status TryPush(const T& value) = 0;

  // 非阻塞弹出一个元素。
  // 返回：
  // - kOk：value 为弹出元素。
  // - kWouldBlock：缓冲区为空。
  virtual api::Result<T> TryPop() = 0;

  // 查看队首元素但不移除。
  virtual api::Status TryPeek(T* out) const = 0;

  // 清空缓冲区。
  virtual api::Status Clear() = 0;

  // 返回当前元素个数。
  virtual std::size_t Size() const = 0;

  // 返回缓冲区总容量。
  virtual std::size_t Capacity() const = 0;

  // 返回当前是否为空。
  virtual bool IsEmpty() const = 0;

  // 返回当前是否已满。
  virtual bool IsFull() const = 0;
};

}  // namespace concurrent
}  // namespace corekit
