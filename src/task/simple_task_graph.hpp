#pragma once

#include <cstdint>
#include <map>
#include <set>
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
  api::Status AddDependency(std::uint64_t before_task_id,
                            std::uint64_t after_task_id) override;
  api::Status Clear() override;
  api::Status Run() override;

 private:
  struct TaskNode {
    std::uint64_t id;
    void (*fn)(void*);
    void* user_data;
  };

  std::map<std::uint64_t, TaskNode> nodes_;
  std::map<std::uint64_t, std::set<std::uint64_t> > edges_;
  std::uint64_t next_id_;
};

}  // namespace task
}  // namespace corekit

