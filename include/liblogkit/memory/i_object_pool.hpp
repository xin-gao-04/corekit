#pragma once

#include <cstddef>
#include <cstdint>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace memory {

template <typename T>
class IObjectPool {
 public:
  virtual ~IObjectPool() {}

  // 返回实现名称，便于区分不同池化策略（固定块/分级块等）。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 预热对象池。
  // 参数：
  // - count: 预创建对象数量。
  // 返回：kOk 表示预热成功；失败会返回明确错误码。
  // 线程安全：建议在启动阶段单线程调用。
  virtual api::Status Reserve(std::size_t count) = 0;

  // 借出一个对象。
  // 返回：
  // - kOk：value 为对象指针。
  // - kWouldBlock / kInternalError：池暂不可用或分配失败。
  // 线程安全：线程安全。
  virtual api::Result<T*> Acquire() = 0;

  // 归还对象到池中。
  // 参数：
  // - obj: 由本池 Acquire 得到的对象。
  // 返回：kOk 表示归还成功。
  // 线程安全：线程安全。
  virtual api::Status ReleaseObject(T* obj) = 0;

  // 当前可借对象数量（近似值）。
  virtual std::size_t Available() const = 0;
};

}  // namespace memory
}  // namespace liblogkit
