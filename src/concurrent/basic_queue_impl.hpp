#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_queue.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

template <typename T>
class BasicMutexQueueImpl : public IQueue<T> {
 public:
  explicit BasicMutexQueueImpl(std::size_t capacity = 0) : capacity_(capacity) {}
  virtual ~BasicMutexQueueImpl() {}

  virtual const char* Name() const { return "corekit.concurrent.basic_mutex_queue"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status TryPush(const T& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (capacity_ > 0 && queue_.size() >= capacity_) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is full");
    }
    queue_.push_back(value);
    return api::Status::Ok();
  }

  virtual api::Status TryPushMove(T&& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (capacity_ > 0 && queue_.size() >= capacity_) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is full");
    }
    queue_.push_back(std::move(value));
    return api::Status::Ok();
  }

  virtual api::Status TryPushBatch(const T* values, std::size_t count, std::size_t* pushed) {
    if (pushed != NULL) *pushed = 0;
    if (values == NULL && count > 0) {
      return api::Status(api::StatusCode::kInvalidArgument, "values is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    std::size_t done = 0;
    while (done < count) {
      if (capacity_ > 0 && queue_.size() >= capacity_) {
        if (pushed != NULL) *pushed = done;
        return api::Status(api::StatusCode::kWouldBlock, "queue is full");
      }
      queue_.push_back(values[done]);
      ++done;
    }
    if (pushed != NULL) *pushed = done;
    return api::Status::Ok();
  }

  virtual api::Result<T> TryPop() {
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
      return api::Result<T>(api::Status(api::StatusCode::kWouldBlock, "queue is empty"));
    }
    T value = queue_.front();
    queue_.pop_front();
    return api::Result<T>(value);
  }

  virtual std::size_t ApproxSize() const {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.size();
  }

  virtual bool IsEmpty() const {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.empty();
  }

  virtual api::Status TryPeek(T* out) const {
    if (out == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "out is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is empty");
    }
    *out = queue_.front();
    return api::Status::Ok();
  }

  virtual api::Status TryPopBatch(T* out_values, std::size_t capacity, std::size_t* popped) {
    if (popped != NULL) *popped = 0;
    if (out_values == NULL && capacity > 0) {
      return api::Status(api::StatusCode::kInvalidArgument, "out_values is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
      return api::Status(api::StatusCode::kWouldBlock, "queue is empty");
    }
    std::size_t done = 0;
    while (done < capacity && !queue_.empty()) {
      out_values[done] = queue_.front();
      queue_.pop_front();
      ++done;
    }
    if (popped != NULL) *popped = done;
    return api::Status::Ok();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    queue_.clear();
    return api::Status::Ok();
  }

  virtual std::size_t Capacity() const { return capacity_; }

 private:
  typedef std::deque<T, corekit::memory::GlobalStlAllocator<T> > DequeType;
  std::size_t capacity_;
  mutable std::mutex mu_;
  DequeType queue_;
};

template <typename T>
using BasicMutexQueue = BasicMutexQueueImpl<T>;

}  // namespace concurrent
}  // namespace corekit
