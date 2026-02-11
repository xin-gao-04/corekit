#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

typedef std::uint64_t TaskId;

enum class TaskPriority : std::uint8_t {
  kLow = 0,
  kNormal = 1,
  kHigh = 2
};

enum class ExecutorPolicy : std::uint8_t {
  kFifo = 0,
  kPriority = 1,
  kFair = 2,
  kHybridFairPriority = 3
};

struct ExecutorOptions {
  std::size_t worker_count = 0;
  std::size_t queue_capacity = 0;
  bool enable_work_stealing = false;
  ExecutorPolicy policy = ExecutorPolicy::kHybridFairPriority;
};

struct TaskSubmitOptions {
  TaskPriority priority = TaskPriority::kNormal;
  std::uint32_t tag = 0;
  std::uint64_t serial_key = 0;  // 0 means no serial-group constraint.
};

struct ExecutorStats {
  std::uint64_t submitted = 0;
  std::uint64_t completed = 0;
  std::uint64_t failed = 0;
  std::uint64_t canceled = 0;
  std::uint64_t rejected = 0;
  std::uint64_t stolen = 0;
  std::size_t queue_depth = 0;
  std::size_t queue_high_watermark = 0;
};

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

  // 提交任务并返回任务 ID，可附带调度选项。
  virtual api::Result<TaskId> SubmitEx(void (*fn)(void*), void* user_data,
                                       const TaskSubmitOptions& options) = 0;

  // 按 key 串行提交任务。同 key 任务不会并发执行。
  virtual api::Result<TaskId> SubmitWithKey(std::uint64_t serial_key,
                                            void (*fn)(void*), void* user_data) = 0;

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

  // 等待指定任务完成。timeout_ms=0 表示无限等待。
  virtual api::Status Wait(TaskId id, std::uint32_t timeout_ms) = 0;

  // 等待任务集合完成。timeout_ms=0 表示无限等待。
  virtual api::Status WaitBatch(const TaskId* ids, std::size_t count,
                                std::uint32_t timeout_ms) = 0;

  // 尝试取消一个尚未运行的任务。
  virtual api::Status TryCancel(TaskId id) = 0;

  // 等待“调用时刻之前提交”的任务全部完成。
  virtual api::Status WaitAllSubmittedBefore() = 0;

  // 等待当前执行器中“此前提交”的任务全部结束。
  // 返回：
  // - kOk：全部任务已完成。
  // - kInternalError：执行器状态异常。
  // 线程安全：线程安全。
  virtual api::Status WaitAll() = 0;

  // 获取执行器运行时统计信息。
  virtual api::Result<ExecutorStats> QueryStats() const = 0;

  // 调整调度策略（线程数不做运行时变更）。
  virtual api::Status Reconfigure(const ExecutorOptions& options) = 0;

  virtual api::Status SetSchedulingPolicy(ExecutorPolicy policy) = 0;
};

}  // namespace task
}  // namespace corekit

