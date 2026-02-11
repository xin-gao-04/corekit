#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <mutex>
#include <string>

#include "corekit/concurrent/i_queue.hpp"

namespace corekit {
namespace concurrent {

template <typename T>
class BasicMutexQueue : public IQueue<T> {
 public:
  explicit BasicMutexQueue(std::size_t capacity = 0) : capacity_(capacity) {}
  virtual ~BasicMutexQueue() {}

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

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    queue_.clear();
    return api::Status::Ok();
  }

  virtual std::size_t Capacity() const { return capacity_; }

 private:
  std::size_t capacity_;
  mutable std::mutex mu_;
  std::deque<T> queue_;
};

}  // namespace concurrent
}  // namespace corekit
