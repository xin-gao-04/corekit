#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"
#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

struct GraphTaskOptions {
  const char* name = NULL;
  TaskPriority priority = TaskPriority::kNormal;
  std::uint64_t serial_key = 0;
};

struct GraphRunOptions {
  bool fail_fast = true;
  std::uint32_t max_concurrency = 0;
};

struct GraphRunStats {
  std::uint64_t total = 0;
  std::uint64_t succeeded = 0;
  std::uint64_t failed = 0;
  std::uint64_t canceled = 0;
};

class ITaskGraph {
 public:
  virtual ~ITaskGraph() {}

  // 返回实现名称。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 新增一个任务节点。
  // 参数：
  // - fn: 任务函数，不允许为空。
  // - user_data: 任务上下文。
  // 返回：
  // - kOk：value 为任务 ID。
  // - kInvalidArgument：参数非法。
  virtual api::Result<std::uint64_t> AddTask(void (*fn)(void*), void* user_data) = 0;

  // 新增带选项的任务节点。
  virtual api::Result<std::uint64_t> AddTaskEx(void (*fn)(void*), void* user_data,
                                               const GraphTaskOptions& options) = 0;

  // 新增依赖关系：before -> after。
  // 返回：kOk 表示依赖添加成功。
  virtual api::Status AddDependency(std::uint64_t before_task_id,
                                    std::uint64_t after_task_id) = 0;

  // 批量新增依赖关系：before[i] -> after。
  virtual api::Status AddDependencies(std::uint64_t after_task_id,
                                      const std::uint64_t* before_task_ids,
                                      std::size_t count) = 0;

  // 校验图结构合法性（包含环检测）。
  virtual api::Status Validate() const = 0;

  // 清空图结构及内部状态。
  virtual api::Status Clear() = 0;

  // 运行任务图直到全部完成。
  // 返回：kOk 表示执行成功。
  virtual api::Status Run() = 0;

  // 使用外部执行器运行任务图。
  virtual api::Result<GraphRunStats> RunWithExecutor(IExecutor* executor,
                                                      const GraphRunOptions& options) = 0;
};

}  // namespace task
}  // namespace corekit
