#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "3party/concurrentqueue.h"
#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_queue.hpp"

namespace corekit {
namespace concurrent {

template <typename T>
class MoodycamelQueueImpl : public IQueue<T> {
 public:
  explicit MoodycamelQueueImpl(std::size_t capacity = 0)
      : queue_(capacity == 0 ? 1024 : capacity), size_(0) {}

  virtual ~MoodycamelQueueImpl() {}

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

  virtual api::Status TryPushBatch(const T* values, std::size_t count, std::size_t* pushed) {
    if (pushed != NULL) *pushed = 0;
    if (values == NULL && count > 0) {
      return api::Status(api::StatusCode::kInvalidArgument, "values is null");
    }
    std::size_t done = 0;
    while (done < count) {
      if (!queue_.try_enqueue(values[done])) {
        if (pushed != NULL) *pushed = done;
        if (done > 0) size_.fetch_add(static_cast<std::int64_t>(done), std::memory_order_relaxed);
        return api::Status(api::StatusCode::kWouldBlock, "queue is full or unavailable");
      }
      ++done;
    }
    if (done > 0) size_.fetch_add(static_cast<std::int64_t>(done), std::memory_order_relaxed);
    if (pushed != NULL) *pushed = done;
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

  virtual api::Status TryPeek(T* out) const {
    if (out == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "out is null");
    }
    (void)out;
    return api::Status(api::StatusCode::kUnsupported,
                       "peek is not supported for this lock-free queue");
  }

  virtual api::Status TryPopBatch(T* out_values, std::size_t capacity, std::size_t* popped) {
    if (popped != NULL) *popped = 0;
    if (out_values == NULL && capacity > 0) {
      return api::Status(api::StatusCode::kInvalidArgument, "out_values is null");
    }
    std::size_t done = queue_.try_dequeue_bulk(out_values, capacity);
    if (done == 0) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is empty");
    }
    size_.fetch_sub(static_cast<std::int64_t>(done), std::memory_order_relaxed);
    if (popped != NULL) *popped = done;
    return api::Status::Ok();
  }

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

template <typename T>
using MoodycamelQueue = MoodycamelQueueImpl<T>;

}  // namespace concurrent
}  // namespace corekit
