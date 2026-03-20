#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

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
  // 工作线程数。0 = 自动（等于硬件并发数）。
  std::size_t worker_count = 0;
  // 队列容量上限。0 = 无限制。
  std::size_t queue_capacity = 0;
  // 调度策略，默认为混合公平优先级策略。
  ExecutorPolicy policy = ExecutorPolicy::kHybridFairPriority;
};

struct TaskSubmitOptions {
  // 任务优先级，影响调度顺序。
  TaskPriority priority = TaskPriority::kNormal;
  // 串行键：同一键值的任务不会并发执行。0 表示无限制。
  std::uint64_t serial_key = 0;
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

// ─────────────────────────────────────────────────────────────────────────────
// IExecutor
//
// 线程池任务调度器接口。
//
// 典型用法：
//   IExecutor* exec = corekit_create_executor();
//   exec->Submit([] { do_work(); });
//   exec->WaitAll();
//   exec->Release();
//
// 生命周期：
//   - 实例由工厂函数创建，调用者持有所有权。
//   - Release() 会等待所有已提交任务完成后再销毁对象。
// ─────────────────────────────────────────────────────────────────────────────
class IExecutor {
 public:
  virtual ~IExecutor() {}

  // 返回实现名称，便于定位运行时使用的是哪种调度后端。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后指针失效。
  virtual void Release() = 0;

  // ── 提交接口（主要 C++ API，接受 lambda / std::function）─────────────────

  // 提交一个无需跟踪的即发任务。
  // 返回：kOk = 已入队；kWouldBlock = 队列已满；kInternalError = 执行器正在关闭。
  // 线程安全。
  virtual api::Status Submit(std::function<void()> fn) = 0;

  // 提交任务并返回 TaskId，可用于 Wait / TryCancel。
  // options 可指定优先级和串行键。线程安全。
  virtual api::Result<TaskId> SubmitEx(std::function<void()> fn,
                                       const TaskSubmitOptions& options) = 0;

  // 按串行键提交任务：同一 serial_key 的任务保证不并发执行。
  // 等价于 SubmitEx(fn, {.serial_key = serial_key})。线程安全。
  virtual api::Result<TaskId> SubmitWithKey(std::uint64_t serial_key,
                                            std::function<void()> fn) = 0;

  // 并行处理范围 [begin, end)，每批最多 grain 个元素。
  // grain = 0 自动调整为 1；grain 越小并行度越高但调度开销也越高。
  // 调用会阻塞直到所有 chunk 完成。线程安全。
  virtual api::Status ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                                  std::function<void(std::size_t)> fn) = 0;

  // ── 同步 / 取消 ───────────────────────────────────────────────────────────

  // 等待指定任务进入完成状态（无论成功、失败还是取消）。
  // timeout_ms = 0 表示无限等待。
  // 返回：kOk = 任务已结束；kWouldBlock = 超时；kNotFound = ID 不存在。
  // ⚠ kOk 仅代表任务已结束，不代表执行成功。如需判断任务成败请调用 IsTaskSucceeded()。
  virtual api::Status Wait(TaskId id, std::uint32_t timeout_ms) = 0;

  // 等待一批任务全部进入完成状态。timeout_ms = 0 表示无限等待。
  virtual api::Status WaitBatch(const TaskId* ids, std::size_t count,
                                std::uint32_t timeout_ms) = 0;

  // 尝试取消尚未开始执行的任务。
  // 返回：kOk = 取消成功；kWouldBlock = 任务已在运行或已完成；kNotFound = ID 不存在。
  virtual api::Status TryCancel(TaskId id) = 0;

  // 等待当前执行器中所有已提交任务全部完成（包括正在执行的）。
  // 返回：kOk = 所有任务已结束；kInternalError = 执行器异常。线程安全。
  virtual api::Status WaitAll() = 0;

  // ── 结果查询 ──────────────────────────────────────────────────────────────

  // 查询已完成任务的执行结果。须在 Wait() 返回 kOk 后调用。
  // 返回：kOk + true  = 任务成功完成
  //        kOk + false = 任务执行时抛出异常，或被取消
  //        kNotFound   = ID 未知（已超出保留窗口或从未提交过）
  virtual api::Result<bool> IsTaskSucceeded(TaskId id) const = 0;

  // 获取执行器运行时统计信息。
  virtual api::Result<ExecutorStats> QueryStats() const = 0;

  // 运行时调整调度参数（仅 queue_capacity 和 policy 生效，worker_count 不做运行时变更）。
  virtual api::Status Reconfigure(const ExecutorOptions& options) = 0;

};

}  // namespace task
}  // namespace corekit
