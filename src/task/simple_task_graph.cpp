#include "task/simple_task_graph.hpp"

#include <queue>

#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

namespace {

struct GraphTaskExecCtx {
  void (*fn)(void*) = NULL;
  void* user_data = NULL;
  bool failed = false;
};

void RunGraphTask(void* user_data) {
  GraphTaskExecCtx* ctx = static_cast<GraphTaskExecCtx*>(user_data);
  if (ctx == NULL || ctx->fn == NULL) return;
  try {
    ctx->fn(ctx->user_data);
  } catch (...) {
    ctx->failed = true;
  }
}

}  // namespace

SimpleTaskGraph::SimpleTaskGraph() : next_id_(1) {}
SimpleTaskGraph::~SimpleTaskGraph() {}

const char* SimpleTaskGraph::Name() const { return "corekit.task.simple_task_graph"; }
std::uint32_t SimpleTaskGraph::ApiVersion() const { return api::kApiVersion; }
void SimpleTaskGraph::Release() { delete this; }

api::Result<std::uint64_t> SimpleTaskGraph::AddTask(void (*fn)(void*), void* user_data) {
  GraphTaskOptions options;
  return AddTaskEx(fn, user_data, options);
}

api::Result<std::uint64_t> SimpleTaskGraph::AddTaskEx(void (*fn)(void*), void* user_data,
                                                       const GraphTaskOptions& options) {
  if (fn == NULL) {
    return api::Result<std::uint64_t>(
        api::Status(api::StatusCode::kInvalidArgument, "fn is null"));
  }
  const std::uint64_t id = next_id_++;
  TaskNode node;
  node.id = id;
  node.fn = fn;
  node.user_data = user_data;
  node.options = GraphTaskOptions();
  node.name = std::string();
  node.options = options;
  if (options.name != NULL) {
    node.name = options.name;
  }
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

api::Status SimpleTaskGraph::AddDependencies(std::uint64_t after_task_id,
                                             const std::uint64_t* before_task_ids,
                                             std::size_t count) {
  if (count > 0 && before_task_ids == NULL) {
    return api::Status(api::StatusCode::kInvalidArgument, "before_task_ids is null");
  }
  for (std::size_t i = 0; i < count; ++i) {
    api::Status st = AddDependency(before_task_ids[i], after_task_id);
    if (!st.ok()) return st;
  }
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::BuildIndegree(std::map<std::uint64_t, std::size_t>* indegree) const {
  if (indegree == NULL) {
    return api::Status(api::StatusCode::kInvalidArgument, "indegree is null");
  }

  indegree->clear();
  for (std::map<std::uint64_t, TaskNode>::const_iterator it = nodes_.begin(); it != nodes_.end();
       ++it) {
    (*indegree)[it->first] = 0;
  }

  for (std::map<std::uint64_t, std::set<std::uint64_t> >::const_iterator eit = edges_.begin();
       eit != edges_.end(); ++eit) {
    for (std::set<std::uint64_t>::const_iterator dst = eit->second.begin();
         dst != eit->second.end(); ++dst) {
      if (indegree->find(*dst) == indegree->end()) {
        return api::Status(api::StatusCode::kInternalError,
                           "edge references missing node");
      }
      ++(*indegree)[*dst];
    }
  }

  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Validate() const {
  std::map<std::uint64_t, std::size_t> indegree;
  api::Status st = BuildIndegree(&indegree);
  if (!st.ok()) return st;

  std::queue<std::uint64_t> ready;
  for (std::map<std::uint64_t, std::size_t>::const_iterator it = indegree.begin();
       it != indegree.end(); ++it) {
    if (it->second == 0) ready.push(it->first);
  }

  std::size_t processed = 0;
  while (!ready.empty()) {
    const std::uint64_t id = ready.front();
    ready.pop();
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
      if (d->second > 0) --d->second;
      if (d->second == 0) ready.push(*dst);
    }
  }

  if (processed != nodes_.size()) {
    return api::Status(api::StatusCode::kInvalidArgument,
                       "task graph contains cycle or unresolved dependency");
  }
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Clear() {
  nodes_.clear();
  edges_.clear();
  next_id_ = 1;
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Run() {
  GraphRunOptions options;
  api::Result<GraphRunStats> result = RunWithExecutor(NULL, options);
  return result.ok() ? api::Status::Ok() : result.status();
}

api::Result<GraphRunStats> SimpleTaskGraph::RunWithExecutor(IExecutor* executor,
                                                            const GraphRunOptions& options) {
  api::Status validate = Validate();
  if (!validate.ok()) {
    return api::Result<GraphRunStats>(validate);
  }

  std::map<std::uint64_t, std::size_t> indegree;
  api::Status indegree_st = BuildIndegree(&indegree);
  if (!indegree_st.ok()) {
    return api::Result<GraphRunStats>(indegree_st);
  }

  std::queue<std::uint64_t> ready;
  for (std::map<std::uint64_t, std::size_t>::const_iterator it = indegree.begin();
       it != indegree.end(); ++it) {
    if (it->second == 0) ready.push(it->first);
  }

  GraphRunStats stats;
  stats.total = static_cast<std::uint64_t>(nodes_.size());
  std::size_t processed = 0;

  while (!ready.empty()) {
    std::vector<std::uint64_t> level;
    const std::size_t level_cap =
        options.max_concurrency == 0 ? static_cast<std::size_t>(-1)
                                     : static_cast<std::size_t>(options.max_concurrency);

    while (!ready.empty() && level.size() < level_cap) {
      level.push_back(ready.front());
      ready.pop();
    }

    bool level_failed = false;

    if (executor == NULL) {
      for (std::size_t i = 0; i < level.size(); ++i) {
        std::map<std::uint64_t, TaskNode>::const_iterator nit = nodes_.find(level[i]);
        if (nit == nodes_.end()) {
          return api::Result<GraphRunStats>(
              api::Status(api::StatusCode::kInternalError, "node missing during execution"));
        }
        try {
          nit->second.fn(nit->second.user_data);
          ++stats.succeeded;
        } catch (...) {
          ++stats.failed;
          level_failed = true;
        }
      }
    } else {
      std::vector<GraphTaskExecCtx> ctx(level.size());
      std::vector<TaskId> ids;
      ids.reserve(level.size());

      for (std::size_t i = 0; i < level.size(); ++i) {
        std::map<std::uint64_t, TaskNode>::const_iterator nit = nodes_.find(level[i]);
        if (nit == nodes_.end()) {
          return api::Result<GraphRunStats>(
              api::Status(api::StatusCode::kInternalError, "node missing during execution"));
        }

        ctx[i].fn = nit->second.fn;
        ctx[i].user_data = nit->second.user_data;
        ctx[i].failed = false;

        TaskSubmitOptions submit_options;
        submit_options.priority = nit->second.options.priority;
        submit_options.serial_key = nit->second.options.serial_key;

        api::Result<TaskId> sub = executor->SubmitEx(&RunGraphTask, &ctx[i], submit_options);
        if (!sub.ok()) {
          if (!ids.empty()) {
            (void)executor->WaitBatch(&ids[0], ids.size(), 0);
          }
          return api::Result<GraphRunStats>(sub.status());
        }
        ids.push_back(sub.value());
      }

      api::Status wait_st = executor->WaitBatch(ids.empty() ? NULL : &ids[0], ids.size(), 0);
      if (!wait_st.ok()) {
        if (!ids.empty()) {
          (void)executor->WaitBatch(&ids[0], ids.size(), 0);
        }
        return api::Result<GraphRunStats>(wait_st);
      }

      for (std::size_t i = 0; i < ctx.size(); ++i) {
        if (ctx[i].failed) {
          ++stats.failed;
          level_failed = true;
        } else {
          ++stats.succeeded;
        }
      }
    }

    processed += level.size();

    if (level_failed && options.fail_fast) {
      return api::Result<GraphRunStats>(
          api::Status(api::StatusCode::kInternalError, "task graph execution failed"));
    }

    for (std::size_t i = 0; i < level.size(); ++i) {
      const std::uint64_t id = level[i];
      std::map<std::uint64_t, std::set<std::uint64_t> >::const_iterator out = edges_.find(id);
      if (out == edges_.end()) continue;
      for (std::set<std::uint64_t>::const_iterator dst = out->second.begin();
           dst != out->second.end(); ++dst) {
        std::map<std::uint64_t, std::size_t>::iterator d = indegree.find(*dst);
        if (d == indegree.end()) {
          return api::Result<GraphRunStats>(api::Status(
              api::StatusCode::kInternalError, "indegree missing for destination node"));
        }
        if (d->second > 0) --d->second;
        if (d->second == 0) ready.push(*dst);
      }
    }
  }

  if (processed != nodes_.size()) {
    return api::Result<GraphRunStats>(
        api::Status(api::StatusCode::kInvalidArgument,
                    "task graph contains cycle or unresolved dependency"));
  }

  stats.canceled = stats.total - stats.succeeded - stats.failed;
  return api::Result<GraphRunStats>(stats);
}

}  // namespace task
}  // namespace corekit
