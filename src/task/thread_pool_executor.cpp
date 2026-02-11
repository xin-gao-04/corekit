#include "task/thread_pool_executor.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

#include "corekit/api/version.hpp"

namespace corekit {
namespace task {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t worker_count)
    : stopping_(false), active_workers_(0), pending_tasks_(0), next_task_id_(1) {
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
  options_.enable_work_stealing = options.enable_work_stealing;
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

api::Status ThreadPoolExecutor::Enqueue(const std::function<void()>& fn) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) {
      return api::Status(api::StatusCode::kInternalError,
                         "executor is stopping, cannot accept new tasks");
    }
    if (options_.queue_capacity > 0 && tasks_.size() >= options_.queue_capacity) {
      ++stats_.rejected;
      return api::Status(api::StatusCode::kWouldBlock, "executor queue is full");
    }
    tasks_.push(fn);
    ++pending_tasks_;
    stats_.queue_depth = QueueDepthLocked();
    if (stats_.queue_depth > stats_.queue_high_watermark) {
      stats_.queue_high_watermark = stats_.queue_depth;
    }
  }
  cv_.notify_one();
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::Submit(void (*fn)(void*), void* user_data) {
  api::Result<TaskId> r = SubmitEx(fn, user_data, TaskSubmitOptions());
  return r.ok() ? api::Status::Ok() : r.status();
}

api::Result<TaskId> ThreadPoolExecutor::SubmitEx(void (*fn)(void*), void* user_data,
                                                 const TaskSubmitOptions&) {
  if (fn == NULL) {
    return api::Result<TaskId>(api::Status(api::StatusCode::kInvalidArgument, "fn is null"));
  }

  TaskId id = 0;
  std::shared_ptr<TaskState> state(new TaskState());
  {
    std::lock_guard<std::mutex> lock(mu_);
    id = NextTaskIdLocked();
    states_[id] = state;
  }

  api::Status st = Enqueue([this, id, fn, user_data, state]() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      state->started = true;
    }
    if (state->canceled) {
      MarkTaskDone(id, false, false);
      return;
    }
    try {
      fn(user_data);
      MarkTaskDone(id, true, false);
    } catch (...) {
      MarkTaskDone(id, false, true);
    }
  });

  if (!st.ok()) {
    std::lock_guard<std::mutex> lock(mu_);
    states_.erase(id);
    return api::Result<TaskId>(st);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    ++stats_.submitted;
  }
  return api::Result<TaskId>(id);
}

api::Result<TaskId> ThreadPoolExecutor::SubmitWithKey(std::uint64_t serial_key,
                                                      void (*fn)(void*), void* user_data) {
  if (fn == NULL) {
    return api::Result<TaskId>(api::Status(api::StatusCode::kInvalidArgument, "fn is null"));
  }
  if (serial_key == 0) return SubmitEx(fn, user_data, TaskSubmitOptions());

  std::shared_ptr<std::mutex> key_mu;
  {
    std::lock_guard<std::mutex> lock(mu_);
    key_mu = serial_key_mu_[serial_key];
    if (!key_mu) {
      key_mu.reset(new std::mutex());
      serial_key_mu_[serial_key] = key_mu;
    }
  }

  TaskId id = 0;
  std::shared_ptr<TaskState> state(new TaskState());
  {
    std::lock_guard<std::mutex> lock(mu_);
    id = NextTaskIdLocked();
    states_[id] = state;
  }

  api::Status st = Enqueue([this, id, fn, user_data, key_mu, state]() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      state->started = true;
    }
    if (state->canceled) {
      MarkTaskDone(id, false, false);
      return;
    }
    try {
      std::lock_guard<std::mutex> serial_lock(*key_mu);
      fn(user_data);
      MarkTaskDone(id, true, false);
    } catch (...) {
      MarkTaskDone(id, false, true);
    }
  });

  if (!st.ok()) {
    std::lock_guard<std::mutex> lock(mu_);
    states_.erase(id);
    return api::Result<TaskId>(st);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    ++stats_.submitted;
  }
  return api::Result<TaskId>(id);
}

api::Status ThreadPoolExecutor::ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                                            void (*fn)(std::size_t, void*), void* user_data) {
  if (fn == NULL) return api::Status(api::StatusCode::kInvalidArgument, "fn is null");
  if (end < begin) return api::Status(api::StatusCode::kInvalidArgument, "end must be >= begin");
  if (begin == end) return api::Status::Ok();
  if (grain == 0) grain = 1;

  for (std::size_t chunk_begin = begin; chunk_begin < end; chunk_begin += grain) {
    const std::size_t chunk_end = std::min(chunk_begin + grain, end);
    api::Status st = Enqueue([fn, user_data, chunk_begin, chunk_end]() {
      for (std::size_t i = chunk_begin; i < chunk_end; ++i) fn(i, user_data);
    });
    if (!st.ok()) return st;
  }
  return WaitAll();
}

api::Status ThreadPoolExecutor::Wait(TaskId id, std::uint32_t timeout_ms) {
  std::shared_ptr<TaskState> state;
  {
    std::lock_guard<std::mutex> lock(mu_);
    typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it = states_.find(id);
    if (it == states_.end()) return api::Status(api::StatusCode::kNotFound, "task id not found");
    state = it->second;
  }

  std::unique_lock<std::mutex> lock(mu_);
  if (timeout_ms == 0) {
    state->cv.wait(lock, [state]() { return state->done; });
    return api::Status::Ok();
  }
  const bool done = state->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                       [state]() { return state->done; });
  return done ? api::Status::Ok() : api::Status(api::StatusCode::kWouldBlock, "wait timeout");
}

api::Status ThreadPoolExecutor::WaitBatch(const TaskId* ids, std::size_t count,
                                          std::uint32_t timeout_ms) {
  if (ids == NULL && count > 0) {
    return api::Status(api::StatusCode::kInvalidArgument, "ids is null");
  }
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < count; ++i) {
    std::uint32_t remain = timeout_ms;
    if (timeout_ms != 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start);
      if (elapsed.count() >= timeout_ms) {
        return api::Status(api::StatusCode::kWouldBlock, "wait batch timeout");
      }
      remain = timeout_ms - static_cast<std::uint32_t>(elapsed.count());
    }
    api::Status st = Wait(ids[i], remain);
    if (!st.ok()) return st;
  }
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::TryCancel(TaskId id) {
  std::lock_guard<std::mutex> lock(mu_);
  typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it = states_.find(id);
  if (it == states_.end()) return api::Status(api::StatusCode::kNotFound, "task id not found");
  if (it->second->started || it->second->done) {
    return api::Status(api::StatusCode::kWouldBlock, "task already running or done");
  }
  it->second->canceled = true;
  ++stats_.canceled;
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::WaitAllSubmittedBefore() {
  TaskId snapshot = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    snapshot = next_task_id_ == 0 ? 0 : (next_task_id_ - 1);
  }
  for (TaskId id = 1; id <= snapshot; ++id) {
    api::Status st = Wait(id, 0);
    if (st.code() == api::StatusCode::kNotFound) continue;
    if (!st.ok()) return st;
  }
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::WaitAll() {
  std::unique_lock<std::mutex> lock(mu_);
  idle_cv_.wait(lock, [this]() { return pending_tasks_ == 0 && active_workers_ == 0; });
  return api::Status::Ok();
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
  options_.enable_work_stealing = options.enable_work_stealing;
  options_.policy = options.policy;
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::SetSchedulingPolicy(ExecutorPolicy policy) {
  std::lock_guard<std::mutex> lock(mu_);
  options_.policy = policy;
  return api::Status::Ok();
}

TaskId ThreadPoolExecutor::NextTaskIdLocked() { return next_task_id_++; }

void ThreadPoolExecutor::MarkTaskDone(TaskId id, bool executed, bool failed) {
  std::lock_guard<std::mutex> lock(mu_);
  typename std::unordered_map<TaskId, std::shared_ptr<TaskState> >::iterator it = states_.find(id);
  if (it == states_.end()) return;
  it->second->done = true;
  it->second->cv.notify_all();
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
      task = tasks_.front();
      tasks_.pop();
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
      if (pending_tasks_ == 0 && active_workers_ == 0) idle_cv_.notify_all();
    }
  }
}

}  // namespace task
}  // namespace corekit
