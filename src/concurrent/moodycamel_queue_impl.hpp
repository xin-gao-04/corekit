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

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kConcurrent)

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
    try {
      if (!queue_.try_enqueue(value)) {
        return CK_STATUS(api::StatusCode::kWouldBlock, "queue is full or unavailable");
      }
      size_.fetch_add(1, std::memory_order_relaxed);
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "queue push failed");
    }
  }

  virtual api::Status TryPushMove(T&& value) {
    try {
      if (!queue_.try_enqueue(std::move(value))) {
        return CK_STATUS(api::StatusCode::kWouldBlock, "queue is full or unavailable");
      }
      size_.fetch_add(1, std::memory_order_relaxed);
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "queue push failed");
    }
  }

  virtual api::Status TryPushBatch(const T* values, std::size_t count, std::size_t* pushed) {
    if (pushed != NULL) *pushed = 0;
    if (values == NULL && count > 0) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "values is null");
    }
    try {
      std::size_t done = 0;
      while (done < count) {
        if (!queue_.try_enqueue(values[done])) {
          if (pushed != NULL) *pushed = done;
          if (done > 0) {
            size_.fetch_add(static_cast<std::int64_t>(done), std::memory_order_relaxed);
          }
          return CK_STATUS(api::StatusCode::kWouldBlock, "queue is full or unavailable");
        }
        ++done;
      }
      if (done > 0) size_.fetch_add(static_cast<std::int64_t>(done), std::memory_order_relaxed);
      if (pushed != NULL) *pushed = done;
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "queue push batch failed");
    }
  }

  virtual api::Result<T> TryPop() {
    try {
      T value;
      if (!queue_.try_dequeue(value)) {
        return api::Result<T>(CK_STATUS(api::StatusCode::kWouldBlock, "queue is empty"));
      }
      size_.fetch_sub(1, std::memory_order_relaxed);
      return api::Result<T>(value);
    } catch (...) {
      return api::Result<T>(CK_STATUS(api::StatusCode::kInternalError, "queue pop failed"));
    }
  }

  virtual std::size_t ApproxSize() const {
    return static_cast<std::size_t>(size_.load(std::memory_order_relaxed));
  }

  virtual bool IsEmpty() const { return ApproxSize() == 0; }

  virtual api::Status TryPeek(T* out) const {
    if (out == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "out is null");
    }
    (void)out;
    return CK_STATUS(api::StatusCode::kUnsupported,
                       "peek is not supported for this lock-free queue");
  }

  virtual api::Status TryPopBatch(T* out_values, std::size_t capacity, std::size_t* popped) {
    if (popped != NULL) *popped = 0;
    if (out_values == NULL && capacity > 0) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "out_values is null");
    }
    try {
      std::size_t done = queue_.try_dequeue_bulk(out_values, capacity);
      if (done == 0) {
        return CK_STATUS(api::StatusCode::kWouldBlock, "queue is empty");
      }
      size_.fetch_sub(static_cast<std::int64_t>(done), std::memory_order_relaxed);
      if (popped != NULL) *popped = done;
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "queue pop batch failed");
    }
  }

  virtual api::Status Clear() {
    try {
      T value;
      while (queue_.try_dequeue(value)) {
        size_.fetch_sub(1, std::memory_order_relaxed);
      }
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "queue clear failed");
    }
  }

  virtual std::size_t Capacity() const { return 0; }

 private:
  moodycamel::ConcurrentQueue<T> queue_;
  std::atomic<std::int64_t> size_;
};

template <typename T>
using MoodycamelQueue = MoodycamelQueueImpl<T>;

#undef CK_STATUS

}  // namespace concurrent
}  // namespace corekit

