#include "task/thread_pool_executor.hpp"

#include <algorithm>
#include <exception>

#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace task {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t worker_count)
    : stopping_(false), active_workers_(0), pending_tasks_(0) {
  std::size_t count = worker_count;
  if (count == 0) {
    count = static_cast<std::size_t>(std::thread::hardware_concurrency());
    if (count == 0) count = 1;
  }
  workers_.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    workers_.push_back(std::thread(&ThreadPoolExecutor::WorkerLoop, this));
  }
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

const char* ThreadPoolExecutor::Name() const { return "liblogkit.task.thread_pool_executor"; }
std::uint32_t ThreadPoolExecutor::ApiVersion() const { return api::kApiVersion; }
void ThreadPoolExecutor::Release() { delete this; }

api::Status ThreadPoolExecutor::Enqueue(const std::function<void()>& fn) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) {
      return api::Status(api::StatusCode::kInternalError,
                         "executor is stopping, cannot accept new tasks");
    }
    tasks_.push(fn);
    ++pending_tasks_;
  }
  cv_.notify_one();
  return api::Status::Ok();
}

api::Status ThreadPoolExecutor::Submit(void (*fn)(void*), void* user_data) {
  if (fn == NULL) {
    return api::Status(api::StatusCode::kInvalidArgument, "fn is null");
  }
  return Enqueue([fn, user_data]() { fn(user_data); });
}

api::Status ThreadPoolExecutor::ParallelFor(std::size_t begin, std::size_t end,
                                            std::size_t grain,
                                            void (*fn)(std::size_t, void*),
                                            void* user_data) {
  if (fn == NULL) {
    return api::Status(api::StatusCode::kInvalidArgument, "fn is null");
  }
  if (end < begin) {
    return api::Status(api::StatusCode::kInvalidArgument, "end must be >= begin");
  }
  if (begin == end) {
    return api::Status::Ok();
  }
  if (grain == 0) {
    grain = 1;
  }

  for (std::size_t chunk_begin = begin; chunk_begin < end; chunk_begin += grain) {
    const std::size_t chunk_end = std::min(chunk_begin + grain, end);
    api::Status st = Enqueue([fn, user_data, chunk_begin, chunk_end]() {
      for (std::size_t i = chunk_begin; i < chunk_end; ++i) {
        fn(i, user_data);
      }
    });
    if (!st.ok()) {
      return st;
    }
  }
  return WaitAll();
}

api::Status ThreadPoolExecutor::WaitAll() {
  std::unique_lock<std::mutex> lock(mu_);
  idle_cv_.wait(lock, [this]() { return pending_tasks_ == 0 && active_workers_ == 0; });
  return api::Status::Ok();
}

void ThreadPoolExecutor::WorkerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) {
        return;
      }
      task = tasks_.front();
      tasks_.pop();
      ++active_workers_;
    }

    try {
      task();
    } catch (...) {
      // Keep worker alive; error reporting can be wired to logger in next iteration.
    }

    {
      std::lock_guard<std::mutex> lock(mu_);
      --active_workers_;
      if (pending_tasks_ > 0) {
        --pending_tasks_;
      }
      if (pending_tasks_ == 0 && active_workers_ == 0) {
        idle_cv_.notify_all();
      }
    }
  }
}

}  // namespace task
}  // namespace liblogkit
