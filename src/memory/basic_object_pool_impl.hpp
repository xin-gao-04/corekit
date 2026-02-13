#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/memory/i_object_pool.hpp"
#include "corekit/memory/i_global_allocator.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace memory {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kMemory)

template <typename T>
class BasicObjectPoolImpl : public IObjectPool<T> {
 public:
  explicit BasicObjectPoolImpl(std::size_t max_cached = 1024) : max_cached_(max_cached) {}
  virtual ~BasicObjectPoolImpl() {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < all_.size(); ++i) {
      DestroyOne(all_[i]);
    }
    all_.clear();
    free_.clear();
    all_set_.clear();
    free_set_.clear();
  }

  virtual const char* Name() const { return "corekit.memory.basic_object_pool"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status Reserve(std::size_t count) {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < count; ++i) {
      api::Result<T*> created = CreateOne();
      if (!created.ok()) {
        return created.status();
      }
      all_.push_back(created.value());
      all_set_.insert(created.value());
      free_.push_back(created.value());
      free_set_.insert(created.value());
    }
    return api::Status::Ok();
  }

  virtual api::Result<T*> Acquire() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!free_.empty()) {
      T* obj = free_.back();
      free_.pop_back();
      free_set_.erase(obj);
      return api::Result<T*>(obj);
    }

    api::Result<T*> created = CreateOne();
    if (!created.ok()) {
      return created;
    }
    all_.push_back(created.value());
    all_set_.insert(created.value());
    return api::Result<T*>(created.value());
  }

  virtual api::Status ReleaseObject(T* obj) {
    if (obj == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "obj is null");
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (all_set_.find(obj) == all_set_.end()) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "object does not belong to this pool");
    }
    if (free_set_.find(obj) != free_set_.end()) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "object already released");
    }
    if (free_.size() >= max_cached_) {
      EraseFromAll(obj);
      DestroyOne(obj);
      return api::Status::Ok();
    }
    free_.push_back(obj);
    free_set_.insert(obj);
    return api::Status::Ok();
  }

  virtual std::size_t Available() const {
    std::lock_guard<std::mutex> lock(mu_);
    return free_.size();
  }

  virtual std::size_t TotalAllocated() const {
    std::lock_guard<std::mutex> lock(mu_);
    return all_.size();
  }

  virtual api::Status Trim(std::size_t keep_free) {
    std::lock_guard<std::mutex> lock(mu_);
    while (free_.size() > keep_free) {
      T* ptr = free_.back();
      free_.pop_back();
      free_set_.erase(ptr);
      EraseFromAll(ptr);
      DestroyOne(ptr);
    }
    return api::Status::Ok();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    if (free_.size() != all_.size()) {
      return CK_STATUS(api::StatusCode::kWouldBlock,
                       "cannot clear pool while objects are still acquired");
    }
    for (std::size_t i = 0; i < all_.size(); ++i) {
      DestroyOne(all_[i]);
    }
    all_.clear();
    free_.clear();
    all_set_.clear();
    free_set_.clear();
    return api::Status::Ok();
  }

 private:
  typedef corekit::memory::GlobalStlAllocator<T*> PtrAlloc;
  typedef std::vector<T*, PtrAlloc> PtrVector;
  typedef std::unordered_set<T*, std::hash<T*>, std::equal_to<T*>, PtrAlloc> PtrSet;

  std::size_t max_cached_;
  mutable std::mutex mu_;
  PtrVector all_;
  PtrVector free_;
  PtrSet all_set_;
  PtrSet free_set_;

  api::Result<T*> CreateOne() {
    const std::size_t alignment = alignof(T) < sizeof(void*) ? sizeof(void*) : alignof(T);
    api::Result<void*> mem = GlobalAllocator::Allocate(sizeof(T), alignment);
    if (!mem.ok() || mem.value() == NULL) {
      return api::Result<T*>(CK_STATUS(api::StatusCode::kInternalError, "pool allocate failed"));
    }
    T* obj = NULL;
    try {
      obj = new (mem.value()) T();
    } catch (...) {
      GlobalAllocator::Deallocate(mem.value());
      return api::Result<T*>(CK_STATUS(api::StatusCode::kInternalError,
                                       "pool object construction failed"));
    }
    return api::Result<T*>(obj);
  }

  void DestroyOne(T* obj) {
    if (obj == NULL) return;
    obj->~T();
    (void)GlobalAllocator::Deallocate(static_cast<void*>(obj));
  }

  void EraseFromAll(T* ptr) {
    all_set_.erase(ptr);
    for (typename PtrVector::iterator it = all_.begin(); it != all_.end(); ++it) {
      if (*it == ptr) {
        all_.erase(it);
        return;
      }
    }
  }
};

template <typename T>
using BasicObjectPool = BasicObjectPoolImpl<T>;

#undef CK_STATUS

}  // namespace memory
}  // namespace corekit
