#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_queue.hpp"
#include "3party/concurrentqueue.h"

namespace corekit {
namespace concurrent {

template <typename T>
class MoodycamelQueue : public IQueue<T> {
 public:
  explicit MoodycamelQueue(std::size_t capacity = 0)
      : queue_(capacity == 0 ? 1024 : capacity), size_(0) {}

  virtual ~MoodycamelQueue() {}

  virtual const char* Name() const { return "corekit.concurrent.moodycamel_queue"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status TryPush(const T& value) {
    if (!queue_.try_enqueue(value)) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is full or unavailable");
    }
    size_.fetch_add(1, std::memory_order_relaxed);
    return api::Status::Ok();
  }

  virtual api::Status TryPushMove(T&& value) {
    if (!queue_.try_enqueue(std::move(value))) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is full or unavailable");
    }
    size_.fetch_add(1, std::memory_order_relaxed);
    return api::Status::Ok();
  }

  virtual api::Result<T> TryPop() {
    T value;
    if (!queue_.try_dequeue(value)) {
      return api::Result<T>(api::Status(api::StatusCode::kWouldBlock, "queue is empty"));
    }
    size_.fetch_sub(1, std::memory_order_relaxed);
    return api::Result<T>(value);
  }

  virtual std::size_t ApproxSize() const {
    return static_cast<std::size_t>(size_.load(std::memory_order_relaxed));
  }

  virtual bool IsEmpty() const { return ApproxSize() == 0; }

  virtual api::Status Clear() {
    T value;
    while (queue_.try_dequeue(value)) {
      size_.fetch_sub(1, std::memory_order_relaxed);
    }
    return api::Status::Ok();
  }

  virtual std::size_t Capacity() const { return 0; }

 private:
  moodycamel::ConcurrentQueue<T> queue_;
  std::atomic<std::int64_t> size_;
};

}  // namespace concurrent
}  // namespace corekit
