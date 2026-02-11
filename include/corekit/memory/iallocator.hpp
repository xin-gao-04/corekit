#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"

namespace corekit {
namespace memory {

enum class AllocBackend { kSystem = 0, kTbbScalable = 1, kMimalloc = 2 };

class IAllocator {
 public:
  virtual ~IAllocator() {}

  // 返回实现名称，便于日志中快速定位当前实际使用的分配器实现。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  // 用途：运行期检查 DLL 与头文件是否匹配。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后指针立即失效。
  virtual void Release() = 0;

  // 切换分配后端。
  // 参数：
  // - backend: 目标分配后端（系统分配器/TBB/mimalloc）。
  // 返回：
  // - kOk：切换成功。
  // - kUnsupported：当前构建未启用该后端。
  // 说明：切换只影响“后续”Allocate，不会自动迁移已分配内存。
  // 线程安全：线程安全。
  virtual api::Status SetBackend(AllocBackend backend) = 0;

  // 分配一块对齐内存。
  // 参数：
  // - size: 请求字节数，必须 > 0。
  // - alignment: 对齐字节数，建议为 2 的幂，且 >= sizeof(void*)。
  // 返回：
  // - kOk：value 为可用指针。
  // - kInvalidArgument：入参不合法。
  // - kInternalError：底层分配失败。
  // 线程安全：线程安全。
  virtual api::Result<void*> Allocate(std::size_t size, std::size_t alignment) = 0;

  // 释放 Allocate 返回的内存块。
  // 参数：
  // - ptr: 由本实例分配得到的地址；允许传入 nullptr（视为 no-op）。
  // 返回：
  // - kOk：释放成功。
  // - kInvalidArgument：指针来源非法（可选实现策略）。
  // 线程安全：线程安全。
  virtual api::Status Deallocate(void* ptr) = 0;
};

}  // namespace memory
}  // namespace corekit

