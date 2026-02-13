#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_ring_buffer.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kConcurrent)

template <typename T>
class BasicRingBufferImpl : public IRingBuffer<T> {
 public:
  static_assert(std::is_default_constructible<T>::value,
                "BasicRingBufferImpl<T> requires T to be default-constructible");

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
      return CK_STATUS(api::StatusCode::kWouldBlock, "ring buffer is full");
    }
    try {
      data_[tail_] = value;
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "ring buffer allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "ring buffer push failed");
    }
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    return api::Status::Ok();
  }

  virtual api::Result<T> TryPop() {
    std::lock_guard<std::mutex> lock(mu_);
    if (size_ == 0) {
      return api::Result<T>(CK_STATUS(api::StatusCode::kWouldBlock, "ring buffer is empty"));
    }
    try {
      T value = std::move(data_[head_]);
      head_ = (head_ + 1) % capacity_;
      --size_;
      return api::Result<T>(value);
    } catch (...) {
      return api::Result<T>(CK_STATUS(api::StatusCode::kInternalError,
                                        "ring buffer pop failed"));
    }
  }

  virtual api::Status TryPeek(T* out) const {
    if (out == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "out is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (size_ == 0) {
      return CK_STATUS(api::StatusCode::kWouldBlock, "ring buffer is empty");
    }
    try {
      *out = data_[head_];
      return api::Status::Ok();
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "ring buffer peek failed");
    }
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

#undef CK_STATUS

}  // namespace concurrent
}  // namespace corekit

