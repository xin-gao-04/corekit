#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_ring_buffer.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

template <typename T>
class BasicRingBufferImpl : public IRingBuffer<T> {
 public:
  explicit BasicRingBufferImpl(std::size_t capacity)
      : capacity_(capacity),
        data_(capacity == 0 ? 1 : capacity),
        head_(0),
        tail_(0),
        size_(0) {}

  virtual ~BasicRingBufferImpl() {}

  virtual const char* Name() const { return "corekit.concurrent.basic_ring_buffer"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status TryPush(const T& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (capacity_ == 0 || size_ >= capacity_) {
      return api::Status(api::StatusCode::kWouldBlock, "ring buffer is full");
    }
    data_[tail_] = value;
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    return api::Status::Ok();
  }

  virtual api::Result<T> TryPop() {
    std::lock_guard<std::mutex> lock(mu_);
    if (size_ == 0) {
      return api::Result<T>(api::Status(api::StatusCode::kWouldBlock, "ring buffer is empty"));
    }
    T value = data_[head_];
    head_ = (head_ + 1) % capacity_;
    --size_;
    return api::Result<T>(value);
  }

  virtual api::Status TryPeek(T* out) const {
    if (out == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "out is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (size_ == 0) {
      return api::Status(api::StatusCode::kWouldBlock, "ring buffer is empty");
    }
    *out = data_[head_];
    return api::Status::Ok();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    return api::Status::Ok();
  }

  virtual std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return size_;
  }

  virtual std::size_t Capacity() const { return capacity_; }

  virtual bool IsEmpty() const {
    std::lock_guard<std::mutex> lock(mu_);
    return size_ == 0;
  }

  virtual bool IsFull() const {
    std::lock_guard<std::mutex> lock(mu_);
    return capacity_ > 0 && size_ >= capacity_;
  }

 private:
  typedef corekit::memory::GlobalStlAllocator<T> ElemAlloc;
  typedef std::vector<T, ElemAlloc> Storage;

  const std::size_t capacity_;
  mutable std::mutex mu_;
  Storage data_;
  std::size_t head_;
  std::size_t tail_;
  std::size_t size_;
};

template <typename T>
using BasicRingBuffer = BasicRingBufferImpl<T>;

}  // namespace concurrent
}  // namespace corekit
