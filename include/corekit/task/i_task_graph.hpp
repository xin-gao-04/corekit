#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"
#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

// 任务节点选项（可选，均有默认值）。
struct GraphTaskOptions {
  // 任务名称，用于调试/日志，可为 NULL。
  const char* name = NULL;
  // 任务调度优先级（通过外部执行器运行时生效）。
  TaskPriority priority = TaskPriority::kNormal;
};

// RunWithExecutor 的运行控制选项。
struct GraphRunOptions {
  // true：任意节点失败后立即停止提交新任务（已在运行的继续执行）。
  bool fail_fast = true;
  // 单批最大并发节点数。0 = 不限制（同一层级的节点全部并发）。
  std::uint32_t max_concurrency = 0;
};

// 每次运行的统计快照。
struct GraphRunStats {
  std::uint64_t total = 0;
  std::uint64_t succeeded = 0;
  std::uint64_t failed = 0;
  std::uint64_t canceled = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// ITaskGraph
//
// 基于 DAG（有向无环图）的任务图接口。
//
// 典型用法：
//   ITaskGraph* g = corekit_create_task_graph();
//   TaskId a = g->AddTask([] { step_a(); }).value();
//   TaskId b = g->AddTask([] { step_b(); }).value();
//   TaskId c = g->AddTask([] { step_c(); }).value();
//   g->AddDependency(a, c);   // a 先于 c
//   g->AddDependency(b, c);   // b 先于 c（a 与 b 并行）
//   g->Validate();
//
//   // 同步单线程运行（无需外部执行器）：
//   g->Run();
//
//   // 使用外部线程池并行运行：
//   g->RunWithExecutor(exec, {}).value();
//
//   g->Release();
// ─────────────────────────────────────────────────────────────────────────────
class ITaskGraph {
 public:
  virtual ~ITaskGraph() {}

  // 返回实现名称。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 新增一个任务节点，返回节点 ID（可用于 AddDependency）。
  // options 为可选配置（名称、优先级）；缺省时使用默认值。
  // 返回：kOk + TaskId = 成功；kInvalidArgument = 参数非法。
  virtual api::Result<TaskId> AddTask(void (*fn)(void*), void* user_data,
                                      const GraphTaskOptions& options = GraphTaskOptions()) = 0;

  // 新增依赖关系：before_task_id 执行完成后才会执行 after_task_id。
  // 返回：kOk = 成功；kNotFound = ID 不存在；kInvalidArgument = 自依赖。
  virtual api::Status AddDependency(TaskId before_task_id, TaskId after_task_id) = 0;

  // 批量新增依赖：before_task_ids[i] -> after_task_id。
  virtual api::Status AddDependencies(TaskId after_task_id,
                                      const TaskId* before_task_ids,
                                      std::size_t count) = 0;

  // 校验图结构合法性（环检测）。
  virtual api::Status Validate() const = 0;

  // 清空图结构及内部状态。Reset 后可重新 AddTask/AddDependency。
  virtual api::Status Clear() = 0;

  // 同步单线程运行任务图（不需要外部执行器）。
  // 等价于 RunWithExecutor(nullptr, {fail_fast=true})，但更简洁。
  // 返回：GraphRunStats 包含执行统计，失败时 failed > 0。
  virtual api::Result<GraphRunStats> Run() = 0;

  // 使用外部执行器并行运行任务图，支持 fail_fast 和并发度控制。
  // executor 不允许为 nullptr；请使用 Run() 进行同步执行。
  // 返回：GraphRunStats 包含执行统计。
  virtual api::Result<GraphRunStats> RunWithExecutor(IExecutor* executor,
                                                     const GraphRunOptions& options) = 0;
};

}  // namespace task
}  // namespace corekit
