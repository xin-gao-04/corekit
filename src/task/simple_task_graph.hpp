#pragma once

#include <cstdint>
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

  api::Result<std::uint64_t> AddTask(void (*fn)(void*), void* user_data) override;
  api::Result<std::uint64_t> AddTaskEx(void (*fn)(void*), void* user_data,
                                       const GraphTaskOptions& options) override;
  api::Status AddDependency(std::uint64_t before_task_id,
                            std::uint64_t after_task_id) override;
  api::Status AddDependencies(std::uint64_t after_task_id,
                              const std::uint64_t* before_task_ids,
                              std::size_t count) override;
  api::Status Validate() const override;
  api::Status Clear() override;
  api::Status Run() override;
  api::Result<GraphRunStats> RunWithExecutor(IExecutor* executor,
                                              const GraphRunOptions& options) override;

 private:
  struct TaskNode {
    std::uint64_t id;
    void (*fn)(void*);
    void* user_data;
    GraphTaskOptions options;
    std::string name;
  };

  api::Status BuildIndegree(std::map<std::uint64_t, std::size_t>* indegree) const;

  std::map<std::uint64_t, TaskNode> nodes_;
  std::map<std::uint64_t, std::set<std::uint64_t> > edges_;
  std::uint64_t next_id_;
};

}  // namespace task
}  // namespace corekit
