#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "corekit/task/iexecutor.hpp"

namespace corekit {
namespace task {

class ThreadPoolExecutor : public IExecutor {
 public:
  explicit ThreadPoolExecutor(std::size_t worker_count = 0);
  ~ThreadPoolExecutor() override;

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status Submit(void (*fn)(void*), void* user_data) override;
  api::Status ParallelFor(std::size_t begin, std::size_t end, std::size_t grain,
                          void (*fn)(std::size_t, void*), void* user_data) override;
  api::Status WaitAll() override;

 private:
  api::Status Enqueue(const std::function<void()>& fn);
  void WorkerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()> > tasks_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable idle_cv_;
  bool stopping_;
  std::size_t active_workers_;
  std::size_t pending_tasks_;
};

}  // namespace task
}  // namespace corekit

