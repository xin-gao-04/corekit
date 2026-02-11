#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "corekit/memory/i_object_pool.hpp"

namespace corekit {
namespace memory {

template <typename T>
class BasicObjectPool : public IObjectPool<T> {
 public:
  explicit BasicObjectPool(std::size_t max_cached = 1024) : max_cached_(max_cached) {}
  virtual ~BasicObjectPool() {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < all_.size(); ++i) {
      delete all_[i];
    }
    all_.clear();
    free_.clear();
  }

  virtual const char* Name() const { return "corekit.memory.basic_object_pool"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status Reserve(std::size_t count) {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < count; ++i) {
      T* obj = new (std::nothrow) T();
      if (obj == NULL) {
        return api::Status(api::StatusCode::kInternalError, "reserve allocation failed");
      }
      all_.push_back(obj);
      free_.push_back(obj);
    }
    return api::Status::Ok();
  }

  virtual api::Result<T*> Acquire() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!free_.empty()) {
      T* obj = free_.back();
      free_.pop_back();
      return api::Result<T*>(obj);
    }

    T* obj = new (std::nothrow) T();
    if (obj == NULL) {
      return api::Result<T*>(
          api::Status(api::StatusCode::kInternalError, "acquire allocation failed"));
    }
    all_.push_back(obj);
    return api::Result<T*>(obj);
  }

  virtual api::Status ReleaseObject(T* obj) {
    if (obj == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "obj is null");
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (free_.size() >= max_cached_) {
      return api::Status::Ok();
    }
    free_.push_back(obj);
    return api::Status::Ok();
  }

  virtual std::size_t Available() const {
    std::lock_guard<std::mutex> lock(mu_);
    return free_.size();
  }

 private:
  std::size_t max_cached_;
  mutable std::mutex mu_;
  std::vector<T*> all_;
  std::vector<T*> free_;
};

}  // namespace memory
}  // namespace corekit
