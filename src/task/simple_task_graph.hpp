#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "corekit/task/i_task_graph.hpp"

namespace corekit {
namespace task {

class SimpleTaskGraph : public ITaskGraph {
 public:
  SimpleTaskGraph();
  ~SimpleTaskGraph() override;

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Result<TaskId> AddTask(std::function<void()> fn,
                              const GraphTaskOptions& options) override;
  api::Status AddDependency(TaskId before_task_id, TaskId after_task_id) override;
  api::Status AddDependencies(TaskId after_task_id,
                              const TaskId* before_task_ids,
                              std::size_t count) override;
  api::Status Validate() const override;
  api::Status Clear() override;
  api::Result<GraphRunStats> Run() override;
  api::Result<GraphRunStats> RunWithExecutor(IExecutor* executor,
                                              const GraphRunOptions& options) override;

 private:
  struct TaskNode {
    TaskId id;
    std::function<void()> fn;
    GraphTaskOptions options;
    std::string name;
  };

  api::Status BuildIndegree(std::map<TaskId, std::size_t>* indegree) const;
  api::Result<GraphRunStats> RunInternal(IExecutor* executor,
                                         const GraphRunOptions& options);

  std::map<TaskId, TaskNode> nodes_;
  std::map<TaskId, std::set<TaskId> > edges_;
  TaskId next_id_;
};

}  // namespace task
}  // namespace corekit
