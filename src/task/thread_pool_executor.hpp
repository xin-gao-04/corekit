#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

class ThreadPoolExecutor : public IExecutor {
 public:
  explicit ThreadPoolExecutor(std::size_t worker_count = 0);
  explicit ThreadPoolExecutor(const ExecutorOptions& options);
  ~ThreadPoolExecutor() override;

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status Submit(void (*fn)(void*), void* user_data) override;
  api::Result<TaskId> SubmitEx(void (*fn)(void*), void* user_data,
                               const TaskSubmitOptions& options) override;
  api::Result<TaskId> SubmitWithKey(std::uint64_t serial_key,
                                    void (*fn)(void*), void* user_data) override;
  api::Status ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                          void (*fn)(std::size_t, void*), void* user_data) override;
  api::Status Wait(TaskId id, std::uint32_t timeout_ms) override;
  api::Status WaitBatch(const TaskId* ids, std::size_t count,
                        std::uint32_t timeout_ms) override;
  api::Status TryCancel(TaskId id) override;
  api::Status WaitAllSubmittedBefore() override;
  api::Status WaitAll() override;
  api::Result<ExecutorStats> QueryStats() const override;
  api::Status Reconfigure(const ExecutorOptions& options) override;
  api::Status SetSchedulingPolicy(ExecutorPolicy policy) override;

 private:
  struct TaskState {
    bool started = false;
    bool done = false;
    bool canceled = false;
    std::condition_variable cv;
  };

  struct TaskEntry {
    std::function<void()> fn;
    TaskPriority priority = TaskPriority::kNormal;
    std::uint64_t seq = 0;
  };

  std::size_t NormalizeWorkerCount(std::size_t worker_count) const;
  api::Status Enqueue(const std::function<void()>& fn,
                      const TaskSubmitOptions& options);
  std::size_t PickNextTaskIndexLocked() const;
  TaskId NextTaskIdLocked();
  void MarkTaskDone(TaskId id, bool executed, bool failed);
  std::size_t QueueDepthLocked() const;
  void WorkerLoop();

  std::vector<std::thread> workers_;
  std::deque<TaskEntry> tasks_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable idle_cv_;
  bool stopping_;
  std::size_t active_workers_;
  std::size_t pending_tasks_;
  TaskId next_task_id_;
  std::uint64_t enqueue_seq_;
  std::size_t max_retained_states_;
  ExecutorStats stats_;
  ExecutorOptions options_;
  std::unordered_map<TaskId, std::shared_ptr<TaskState> > states_;
  std::set<TaskId> pending_ids_;
  std::deque<TaskId> done_ids_;
  std::unordered_map<std::uint64_t, std::shared_ptr<std::mutex> > serial_key_mu_;
};

}  // namespace task
}  // namespace corekit
