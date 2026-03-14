#include "task/simple_task_graph.hpp"

#include <queue>

#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kTask)

SimpleTaskGraph::SimpleTaskGraph() : next_id_(1) {}
SimpleTaskGraph::~SimpleTaskGraph() {}

const char* SimpleTaskGraph::Name() const { return "corekit.task.simple_task_graph"; }
std::uint32_t SimpleTaskGraph::ApiVersion() const { return api::kApiVersion; }
void SimpleTaskGraph::Release() { delete this; }

// ── AddTask / AddDependency ────────────────────────────────────────────────────

api::Result<TaskId> SimpleTaskGraph::AddTask(std::function<void()> fn,
                                              const GraphTaskOptions& options) {
  if (!fn) {
    return api::Result<TaskId>(
        CK_STATUS(api::StatusCode::kInvalidArgument, "fn is null"));
  }
  const TaskId id = next_id_++;
  TaskNode node;
  node.id = id;
  node.fn = std::move(fn);
  node.options = options;
  if (options.name != NULL) node.name = options.name;
  nodes_[id] = node;
  edges_[id];  // ensure entry exists even if no outgoing edges
  return api::Result<TaskId>(id);
}

api::Status SimpleTaskGraph::AddDependency(TaskId before_task_id, TaskId after_task_id) {
  if (before_task_id == after_task_id) {
    return CK_STATUS(api::StatusCode::kInvalidArgument, "self dependency is not allowed");
  }
  if (nodes_.find(before_task_id) == nodes_.end() ||
      nodes_.find(after_task_id) == nodes_.end()) {
    return CK_STATUS(api::StatusCode::kNotFound, "task id not found");
  }
  edges_[before_task_id].insert(after_task_id);
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::AddDependencies(TaskId after_task_id,
                                              const TaskId* before_task_ids,
                                              std::size_t count) {
  if (count > 0 && before_task_ids == NULL) {
    return CK_STATUS(api::StatusCode::kInvalidArgument, "before_task_ids is null");
  }
  for (std::size_t i = 0; i < count; ++i) {
    api::Status st = AddDependency(before_task_ids[i], after_task_id);
    if (!st.ok()) return st;
  }
  return api::Status::Ok();
}

// ── Validate / Clear ──────────────────────────────────────────────────────────

api::Status SimpleTaskGraph::BuildIndegree(std::map<TaskId, std::size_t>* indegree) const {
  if (indegree == NULL) {
    return CK_STATUS(api::StatusCode::kInvalidArgument, "indegree is null");
  }
  indegree->clear();
  for (std::map<TaskId, TaskNode>::const_iterator it = nodes_.begin();
       it != nodes_.end(); ++it) {
    (*indegree)[it->first] = 0;
  }
  for (std::map<TaskId, std::set<TaskId> >::const_iterator eit = edges_.begin();
       eit != edges_.end(); ++eit) {
    for (std::set<TaskId>::const_iterator dst = eit->second.begin();
         dst != eit->second.end(); ++dst) {
      if (indegree->find(*dst) == indegree->end()) {
        return CK_STATUS(api::StatusCode::kInternalError, "edge references missing node");
      }
      ++(*indegree)[*dst];
    }
  }
  return api::Status::Ok();
}

api::Status SimpleTaskGraph::Validate() const {
  std::map<TaskId, std::size_t> indegree;
  api::Status st = BuildIndegree(&indegree);
  if (!st.ok()) return st;

  std::queue<TaskId> ready;
  for (std::map<TaskId, std::size_t>::const_iterator it = indegree.begin();
       it != indegree.end(); ++it) {
    if (it->second == 0) ready.push(it->first);
  }

  std::size_t processed = 0;
  while (!ready.empty()) {
    const TaskId id = ready.front();
    ready.pop();
    ++processed;

    std::map<TaskId, std::set<TaskId> >::const_iterator out = edges_.find(id);
    if (out == edges_.end()) continue;
    for (std::set<TaskId>::const_iterator dst = out->second.begin();
         dst != out->second.end(); ++dst) {
      std::map<TaskId, std::size_t>::iterator d = indegree.find(*dst);
      if (d == indegree.end()) {
        return CK_STATUS(api::StatusCode::kInternalError,
                         "indegree missing for destination node");
      }
      if (d->second > 0) --d->second;
      if (d->second == 0) ready.push(*dst);
    }
  }

  if (processed != nodes_.size()) {
    return CK_STATUS(api::StatusCode::kInvalidArgument,
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

// ── Run / RunWithExecutor ─────────────────────────────────────────────────────

api::Result<GraphRunStats> SimpleTaskGraph::Run() {
  // 同步单线程执行（NULL executor 触发内联执行路径）
  GraphRunOptions opts;
  opts.fail_fast = true;
  opts.max_concurrency = 0;
  return RunInternal(NULL, opts);
}

api::Result<GraphRunStats> SimpleTaskGraph::RunWithExecutor(IExecutor* executor,
                                                            const GraphRunOptions& options) {
  if (executor == NULL) {
    return api::Result<GraphRunStats>(
        CK_STATUS(api::StatusCode::kInvalidArgument,
                  "executor is null; use Run() for synchronous execution"));
  }
  return RunInternal(executor, options);
}

api::Result<GraphRunStats> SimpleTaskGraph::RunInternal(IExecutor* executor,
                                                         const GraphRunOptions& options) {
  api::Status validate = Validate();
  if (!validate.ok()) return api::Result<GraphRunStats>(validate);

  std::map<TaskId, std::size_t> indegree;
  api::Status indegree_st = BuildIndegree(&indegree);
  if (!indegree_st.ok()) return api::Result<GraphRunStats>(indegree_st);

  std::queue<TaskId> ready;
  for (std::map<TaskId, std::size_t>::const_iterator it = indegree.begin();
       it != indegree.end(); ++it) {
    if (it->second == 0) ready.push(it->first);
  }

  GraphRunStats stats;
  stats.total = static_cast<std::uint64_t>(nodes_.size());
  std::size_t processed = 0;

  while (!ready.empty()) {
    // 收集当前层（受 max_concurrency 限制）
    const std::size_t level_cap =
        options.max_concurrency == 0
            ? static_cast<std::size_t>(-1)
            : static_cast<std::size_t>(options.max_concurrency);

    std::vector<TaskId> level;
    while (!ready.empty() && level.size() < level_cap) {
      level.push_back(ready.front());
      ready.pop();
    }

    bool level_failed = false;

    if (executor == NULL) {
      // 同步内联执行
      for (std::size_t i = 0; i < level.size(); ++i) {
        std::map<TaskId, TaskNode>::const_iterator nit = nodes_.find(level[i]);
        if (nit == nodes_.end()) {
          return api::Result<GraphRunStats>(
              CK_STATUS(api::StatusCode::kInternalError, "node missing during execution"));
        }
        try {
          nit->second.fn();
          ++stats.succeeded;
        } catch (...) {
          ++stats.failed;
          level_failed = true;
        }
      }
    } else {
      // 通过执行器并行执行当前层
      struct NodeCtx {
        std::function<void()> fn;
        bool failed;
      };
      std::vector<NodeCtx> ctx(level.size());
      std::vector<TaskId> ids;
      ids.reserve(level.size());

      for (std::size_t i = 0; i < level.size(); ++i) {
        std::map<TaskId, TaskNode>::const_iterator nit = nodes_.find(level[i]);
        if (nit == nodes_.end()) {
          return api::Result<GraphRunStats>(
              CK_STATUS(api::StatusCode::kInternalError, "node missing during execution"));
        }
        ctx[i].fn = nit->second.fn;
        ctx[i].failed = false;

        NodeCtx* ctxp = &ctx[i];
        TaskSubmitOptions submit_opts;
        submit_opts.priority = nit->second.options.priority;

        api::Result<TaskId> sub = executor->SubmitEx(
            [ctxp]() {
              try {
                ctxp->fn();
              } catch (...) {
                ctxp->failed = true;
              }
            },
            submit_opts);

        if (!sub.ok()) {
          if (!ids.empty()) (void)executor->WaitBatch(&ids[0], ids.size(), 0);
          return api::Result<GraphRunStats>(sub.status());
        }
        ids.push_back(sub.value());
      }

      api::Status wait_st =
          executor->WaitBatch(ids.empty() ? NULL : &ids[0], ids.size(), 0);
      if (!wait_st.ok()) {
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
      stats.canceled = stats.total - stats.succeeded - stats.failed;
      return api::Result<GraphRunStats>(stats);
    }

    // 更新 indegree，解锁下一层节点
    for (std::size_t i = 0; i < level.size(); ++i) {
      std::map<TaskId, std::set<TaskId> >::const_iterator out = edges_.find(level[i]);
      if (out == edges_.end()) continue;
      for (std::set<TaskId>::const_iterator dst = out->second.begin();
           dst != out->second.end(); ++dst) {
        std::map<TaskId, std::size_t>::iterator d = indegree.find(*dst);
        if (d == indegree.end()) {
          return api::Result<GraphRunStats>(
              CK_STATUS(api::StatusCode::kInternalError,
                        "indegree missing for destination node"));
        }
        if (d->second > 0) --d->second;
        if (d->second == 0) ready.push(*dst);
      }
    }
  }

  if (processed != nodes_.size()) {
    return api::Result<GraphRunStats>(
        CK_STATUS(api::StatusCode::kInvalidArgument,
                  "task graph contains cycle or unresolved dependency"));
  }

  stats.canceled = stats.total - stats.succeeded - stats.failed;
  return api::Result<GraphRunStats>(stats);
}

#undef CK_STATUS

}  // namespace task
}  // namespace corekit
