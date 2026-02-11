#include "task/simple_task_graph.hpp"

#include <queue>

#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

SimpleTaskGraph::SimpleTaskGraph() : next_id_(1) {}
SimpleTaskGraph::~SimpleTaskGraph() {}

const char* SimpleTaskGraph::Name() const { return "corekit.task.simple_task_graph"; }
std::uint32_t SimpleTaskGraph::ApiVersion() const { return api::kApiVersion; }
void SimpleTaskGraph::Release() { delete this; }

api::Result<std::uint64_t> SimpleTaskGraph::AddTask(void (*fn)(void*), void* user_data) {
  if (fn == NULL) {
    return api::Result<std::uint64_t>(
        api::Status(api::StatusCode::kInvalidArgument, "fn is null"));
  }
  const std::uint64_t id = next_id_++;
  TaskNode node;
  node.id = id;
  node.fn = fn;
  node.user_data = user_data;
  nodes_[id] = node;
  edges_[id];
  return api::Result<std::uint64_t>(id);
}

api::Status SimpleTaskGraph::AddDependency(std::uint64_t before_task_id,
                                           std::uint64_t after_task_id) {
  if (before_task_id == after_task_id) {
    return api::Status(api::StatusCode::kInvalidArgument,
                       "self dependency is not allowed");
  }
  if (nodes_.find(before_task_id) == nodes_.end() ||
      nodes_.find(after_task_id) == nodes_.end()) {
    return api::Status(api::StatusCode::kNotFound, "task id not found");
  }
  edges_[before_task_id].insert(after_task_id);
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Clear() {
  nodes_.clear();
  edges_.clear();
  next_id_ = 1;
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Run() {
  std::map<std::uint64_t, std::size_t> indegree;
  for (std::map<std::uint64_t, TaskNode>::const_iterator it = nodes_.begin();
       it != nodes_.end(); ++it) {
    indegree[it->first] = 0;
  }

  for (std::map<std::uint64_t, std::set<std::uint64_t> >::const_iterator eit = edges_.begin();
       eit != edges_.end(); ++eit) {
    for (std::set<std::uint64_t>::const_iterator dst = eit->second.begin();
         dst != eit->second.end(); ++dst) {
      if (indegree.find(*dst) == indegree.end()) {
        return api::Status(api::StatusCode::kInternalError,
                           "edge references missing node");
      }
      ++indegree[*dst];
    }
  }

  std::queue<std::uint64_t> ready;
  for (std::map<std::uint64_t, std::size_t>::const_iterator it = indegree.begin();
       it != indegree.end(); ++it) {
    if (it->second == 0) {
      ready.push(it->first);
    }
  }

  std::size_t processed = 0;
  while (!ready.empty()) {
    const std::uint64_t id = ready.front();
    ready.pop();

    std::map<std::uint64_t, TaskNode>::const_iterator nit = nodes_.find(id);
    if (nit == nodes_.end()) {
      return api::Status(api::StatusCode::kInternalError,
                         "node missing during execution");
    }

    try {
      nit->second.fn(nit->second.user_data);
    } catch (...) {
      return api::Status(api::StatusCode::kInternalError,
                         "task function threw exception");
    }
    ++processed;

    std::map<std::uint64_t, std::set<std::uint64_t> >::const_iterator out = edges_.find(id);
    if (out == edges_.end()) continue;

    for (std::set<std::uint64_t>::const_iterator dst = out->second.begin();
         dst != out->second.end(); ++dst) {
      std::map<std::uint64_t, std::size_t>::iterator d = indegree.find(*dst);
      if (d == indegree.end()) {
        return api::Status(api::StatusCode::kInternalError,
                           "indegree missing for destination node");
      }
      if (d->second > 0) {
        --d->second;
      }
      if (d->second == 0) {
        ready.push(*dst);
      }
    }
  }

  if (processed != nodes_.size()) {
    return api::Status(api::StatusCode::kInvalidArgument,
                       "task graph contains cycle or unresolved dependency");
  }

  return api::Status::Ok();
}

}  // namespace task
}  // namespace corekit


