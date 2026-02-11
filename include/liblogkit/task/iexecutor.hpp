#pragma once

#include <cstddef>
#include <cstdint>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace task {

class IExecutor {
 public:
  virtual ~IExecutor() {}

  // 返回实现名称，便于定位运行时使用的是哪种调度后端。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后指针失效。
  virtual void Release() = 0;

  // 提交一个异步任务。
  // 参数：
  // - fn: 任务函数指针，不允许为 nullptr。
  // - user_data: 透传给 fn 的用户上下文。
  // 返回：
  // - kOk：任务已入调度队列。
  // - kInvalidArgument：fn 为空。
  // 线程安全：线程安全。
  virtual api::Status Submit(void (*fn)(void*), void* user_data) = 0;

  // 并行 for，处理范围 [begin, end)。
  // 参数：
  // - grain: 最小切分粒度，建议 >= 1；越小并行度越高但调度开销也更高。
  // - fn: 每个 index 的处理函数，不允许为 nullptr。
  // 返回：
  // - kOk：任务已完成或成功提交并等待完成（由实现定义）。
  // - kInvalidArgument：区间或函数参数非法。
  // 线程安全：线程安全。
  virtual api::Status ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                                  void (*fn)(std::size_t, void*), void* user_data) = 0;

  // 等待当前执行器中“此前提交”的任务全部结束。
  // 返回：
  // - kOk：全部任务已完成。
  // - kInternalError：执行器状态异常。
  // 线程安全：线程安全。
  virtual api::Status WaitAll() = 0;
};

}  // namespace task
}  // namespace liblogkit
