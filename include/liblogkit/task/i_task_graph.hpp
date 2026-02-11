#pragma once

#include <cstddef>
#include <cstdint>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace task {

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

  // 新增依赖关系：before -> after。
  // 返回：kOk 表示依赖添加成功。
  virtual api::Status AddDependency(std::uint64_t before_task_id,
                                    std::uint64_t after_task_id) = 0;

  // 清空图结构及内部状态。
  virtual api::Status Clear() = 0;

  // 运行任务图直到全部完成。
  // 返回：kOk 表示执行成功。
  virtual api::Status Run() = 0;
};

}  // namespace task
}  // namespace liblogkit
