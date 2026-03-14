#include "task/thread_pool_executor.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kTask)

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t worker_count)
    : stopping_(false),
      active_workers_(0),
      pending_tasks_(0),
      next_task_id_(1),
      enqueue_seq_(0),
      max_retained_states_(65536) {
  options_.worker_count = NormalizeWorkerCount(worker_count);
  options_.policy = ExecutorPolicy::kHybridFairPriority;
  workers_.reserve(options_.worker_count);
  for (std::size_t i = 0; i < options_.worker_count; ++i) {
    workers_.push_back(std::thread(&ThreadPoolExecutor::WorkerLoop, this));
  }
}

ThreadPoolExecutor::ThreadPoolExecutor(const ExecutorOptions& options)
    : ThreadPoolExecutor(options.worker_count) {
  options_.queue_capacity = options.queue_capacity;
  options_.policy = options.policy;
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (std::size_t i = 0; i < workers_.size(); ++i) {
    if (workers_[i].joinable()) workers_[i].join();
  }
}

const char* ThreadPoolExecutor::Name() const { return "corekit.task.thread_pool_executor"; }
std::uint32_t ThreadPoolExecutor::ApiVersion() const { return api::kApiVersion; }
void ThreadPoolExecutor::Release() { delete this; }

std::size_t ThreadPoolExecutor::NormalizeWorkerCount(std::size_t worker_count) const {
  if (worker_count > 0) return worker_count;
  std::size_t count = static_cast<std::size_t>(std::thread::hardware_concurrency());
  return count == 0 ? 1 : count;
}

api::Status ThreadPoolExecutor::Enqueue(std::function<void()> fn,
                                        const TaskSubmitOptions& options) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) {
      return CK_STATUS(api::StatusCode::kInternalError,
                       "executor is stopping, cannot accept new tasks");
    }
    if (options_.queue_capacity > 0 && tasks_.size() >= options_.queue_capacity) {
      ++stats_.rejected;
      return CK_STATUS(api::StatusCode::kWouldBlock, "executor queue is full");
    }
    TaskEntry entry;
    entry.fn = std::move(fn);
    entry.priority = options.priority;
    entry.seq = ++enqueue_seq_;
    tasks_.push_back(std::move(entry));
    ++pending_tasks_;
    ++stats_.submitted;
    stats_.queue_depth = QueueDepthLocked();
    if (stats_.queue_depth > stats_.queue_high_watermark) {
      stats_.queue_high_watermark = stats_.queue_depth;
    }
  }
  cv_.notify_one();
  return api::Status::Ok();
}

std::size_t ThreadPoolExecutor::PickNextTaskIndexLocked() const {
  if (tasks_.empty()) return 0;
  if (options_.policy == ExecutorPolicy::kFifo || options_.policy == ExecutorPolicy::kFair) {
    return 0;
  }

  // kPriority / kHybridFairPriority: highest priority first, FIFO within same priority.
  std::size_t best = 0;
  auto score = [](TaskPriority p) -> int {
    if (p == TaskPriority::kHigh) return 2;
    if (p == TaskPriority::kNormal) return 1;
    return 0;
  };
  int best_score = score(tasks_[0].priority);
  std::uint64_t best_seq = tasks_[0].seq;
  for (std::size_t i = 1; i < tasks_.size(); ++i) {
    const int s = score(tasks_[i].priority);
    if (s > best_score || (s == best_score && tasks_[i].seq < best_seq)) {
      best = i;
      best_score = s;
      best_seq = tasks_[i].seq;
    }
  }
  return best;
}

// ── Submit / SubmitEx / SubmitWithKey / ParallelFor ───────────────────────────

api::Status ThreadPoolExecutor::Submit(std::function<void()> fn) {
  api::Result<TaskId> r = SubmitEx(std::move(fn), TaskSubmitOptions());
  return r.ok() ? api::Status::Ok() : r.status();
}

api::Result<TaskId> ThreadPoolExecutor::SubmitEx(std::function<void()> fn,
                                                 const TaskSubmitOptions& options) {
  if (!fn) {
    return api::Result<TaskId>(CK_STATUS(api::StatusCode::kInvalidArgument, "fn is empty"));
  }

  std::shared_ptr<std::mutex> key_mu;
  if (options.serial_key != 0) {
    std::lock_guard<std::mutex> lock(mu_);
    key_mu = serial_key_mu_[options.serial_key];
    if (!key_mu) {
      key_mu.reset(new std::mutex());
      serial_key_mu_[options.serial_key] = key_mu;
    }
  }

  TaskId id = 0;
  std::shared_ptr<TaskState> state(new TaskState());
  {
    std::lock_guard<std::mutex> lock(mu_);
    id = NextTaskIdLocked();
    states_[id] = state;
    pending_ids_.insert(id);
  }

  api::Status st = Enqueue([this, id, fn, state, key_mu]() {
    bool canceled = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      state->started = true;
      canceled = state->canceled;
    }

    if (canceled) {
      MarkTaskDone(id, false, false);
      return;
    }

    try {
      if (key_mu) {
        std::lock_guard<std::mutex> serial_lock(*key_mu);
        fn();
      } else {
        fn();
      }
      MarkTaskDone(id, true, false);
    } catch (...) {
      MarkTaskDone(id, false, true);
    }
  }, options);

  if (!st.ok()) {
    std::lock_guard<std::mutex> lock(mu_);
    states_.erase(id);
    pending_ids_.erase(id);
    return api::Result<TaskId>(st);
  }

  return api::Result<TaskId>(id);
}

api::Result<TaskId> ThreadPoolExecutor::SubmitWithKey(std::uint64_t serial_key,
                                                      std::function<void()> fn) {
  TaskSubmitOptions options;
  options.serial_key = serial_key;
  return SubmitEx(std::move(fn), options);
}

api::Status ThreadPoolExecutor::ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                                            std::function<void(std::size_t)> fn) {
  if (!fn) return CK_STATUS(api::StatusCode::kInvalidArgument, "fn is empty");
  if (end < begin) return CK_STATUS(api::StatusCode::kInvalidArgument, "end must be >= begin");
  if (begin == end) return api::Status::Ok();
  if (grain == 0) grain = 1;

  std::vector<TaskId> ids;
  for (std::size_t chunk_begin = begin; chunk_begin < end; chunk_begin += grain) {
    const std::size_t chunk_end = std::min(chunk_begin + grain, end);
    // Capture fn by value (shared_ptr-like copy via std::function's ref-counted capture)
    api::Result<TaskId> sub = SubmitEx(
        [fn, chunk_begin, chunk_end]() {
          for (std::size_t i = chunk_begin; i < chunk_end; ++i) fn(i);
        },
        TaskSubmitOptions());
    if (!sub.ok()) {
      if (!ids.empty()) (void)WaitBatch(&ids[0], ids.size(), 0);
      return sub.status();
    }
    ids.push_back(sub.value());
  }

  return ids.empty() ? api::Status::Ok() : WaitBatch(&ids[0], ids.size(), 0);
}

// ── Wait / WaitBatch / TryCancel / WaitAll ────────────────────────────────────

api::Status ThreadPoolExecutor::Wait(TaskId id, std::uint32_t timeout_ms) {
  std::shared_ptr<TaskState> state;
  {
    std::lock_guard<std::mutex> lock(mu_);
    typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it =
        states_.find(id);
    if (it == states_.end())
      return CK_STATUS(api::StatusCode::kNotFound, "task id not found");
    state = it->second;
  }

  std::unique_lock<std::mutex> lock(mu_);
  if (timeout_ms == 0) {
    state->cv.wait(lock, [state]() { return state->done; });
    return api::Status::Ok();
  }
  const bool done = state->cv.wait_for(
      lock, std::chrono::milliseconds(timeout_ms), [state]() { return state->done; });
  return done ? api::Status::Ok() : CK_STATUS(api::StatusCode::kWouldBlock, "wait timeout");
}

api::Status ThreadPoolExecutor::WaitBatch(const TaskId* ids, std::size_t count,
                                          std::uint32_t timeout_ms) {
  if (ids == NULL && count > 0) {
    return CK_STATUS(api::StatusCode::kInvalidArgument, "ids is null");
  }
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < count; ++i) {
    std::uint32_t remain = timeout_ms;
    if (timeout_ms != 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start);
      if (elapsed.count() >= timeout_ms)
        return CK_STATUS(api::StatusCode::kWouldBlock, "wait batch timeout");
      remain = timeout_ms - static_cast<std::uint32_t>(elapsed.count());
    }
    api::Status st = Wait(ids[i], remain);
    if (!st.ok()) return st;
  }
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::TryCancel(TaskId id) {
  std::lock_guard<std::mutex> lock(mu_);
  typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it =
      states_.find(id);
  if (it == states_.end())
    return CK_STATUS(api::StatusCode::kNotFound, "task id not found");
  if (it->second->started || it->second->done)
    return CK_STATUS(api::StatusCode::kWouldBlock, "task already running or done");
  it->second->canceled = true;
  ++stats_.canceled;
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::WaitAll() {
  std::unique_lock<std::mutex> lock(mu_);
  idle_cv_.wait(lock, [this]() { return pending_tasks_ == 0 && active_workers_ == 0; });
  return api::Status::Ok();
}

// ── IsTaskSucceeded / QueryStats / Reconfigure ───────────────────────────────

api::Result<bool> ThreadPoolExecutor::IsTaskSucceeded(TaskId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::const_iterator it =
      states_.find(id);
  if (it == states_.end())
    return api::Result<bool>(CK_STATUS(api::StatusCode::kNotFound,
                                       "task id not found or result expired"));
  return api::Result<bool>(it->second->succeeded);
}

api::Result<ExecutorStats> ThreadPoolExecutor::QueryStats() const {
  std::lock_guard<std::mutex> lock(mu_);
  ExecutorStats out = stats_;
  out.queue_depth = QueueDepthLocked();
  return api::Result<ExecutorStats>(out);
}

api::Status ThreadPoolExecutor::Reconfigure(const ExecutorOptions& options) {
  std::lock_guard<std::mutex> lock(mu_);
  options_.queue_capacity = options.queue_capacity;
  options_.policy = options.policy;
  return api::Status::Ok();
}

// ── Internal helpers ──────────────────────────────────────────────────────────

TaskId ThreadPoolExecutor::NextTaskIdLocked() { return next_task_id_++; }

void ThreadPoolExecutor::MarkTaskDone(TaskId id, bool executed, bool failed) {
  std::lock_guard<std::mutex> lock(mu_);
  typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it =
      states_.find(id);
  if (it == states_.end()) return;

  it->second->done = true;
  it->second->succeeded = executed && !failed;
  it->second->cv.notify_all();
  pending_ids_.erase(id);

  done_ids_.push_back(id);
  while (done_ids_.size() > max_retained_states_) {
    const TaskId old_id = done_ids_.front();
    done_ids_.pop_front();
    typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator old =
        states_.find(old_id);
    if (old != states_.end() && old->second->done) {
      states_.erase(old);
    }
  }

  if (it->second->canceled) return;
  if (failed) {
    ++stats_.failed;
  } else if (executed) {
    ++stats_.completed;
  }
}

std::size_t ThreadPoolExecutor::QueueDepthLocked() const { return tasks_.size(); }

void ThreadPoolExecutor::WorkerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) return;
      const std::size_t idx = PickNextTaskIndexLocked();
      task = std::move(tasks_[idx].fn);
      tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(idx));
      ++active_workers_;
    }

    try {
      task();
    } catch (...) {
      std::lock_guard<std::mutex> lock(mu_);
      ++stats_.failed;
    }

    {
      std::lock_guard<std::mutex> lock(mu_);
      --active_workers_;
      if (pending_tasks_ > 0) --pending_tasks_;
      stats_.queue_depth = QueueDepthLocked();
      idle_cv_.notify_all();
    }
  }
}

#undef CK_STATUS

}  // namespace task
}  // namespace corekit
