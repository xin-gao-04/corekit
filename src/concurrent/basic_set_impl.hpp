#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <unordered_set>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_set.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kConcurrent)

template <typename K>
class BasicConcurrentSetImpl : public IConcurrentSet<K> {
 public:
  BasicConcurrentSetImpl() {}
  virtual ~BasicConcurrentSetImpl() {}

  virtual const char* Name() const { return "corekit.concurrent.basic_set"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status Insert(const K& key) {
    std::lock_guard<std::mutex> lock(mu_);
    if (set_.find(key) != set_.end()) {
      return CK_STATUS(api::StatusCode::kWouldBlock, "key already exists");
    }
    try {
      set_.insert(key);
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "set allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "set insert failed");
    }
    return api::Status::Ok();
  }

  virtual api::Status Erase(const K& key) {
    std::lock_guard<std::mutex> lock(mu_);
    typename SetType::iterator it = set_.find(key);
    if (it == set_.end()) {
      return CK_STATUS(api::StatusCode::kNotFound, "key not found");
    }
    set_.erase(it);
    return api::Status::Ok();
  }

  virtual bool Contains(const K& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    return set_.find(key) != set_.end();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    set_.clear();
    return api::Status::Ok();
  }

  virtual api::Status Reserve(std::size_t expected_size) {
    std::lock_guard<std::mutex> lock(mu_);
    try {
      set_.reserve(expected_size);
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "set reserve failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "set reserve failed");
    }
    return api::Status::Ok();
  }

  virtual api::Status Snapshot(std::vector<K>* keys) const {
    if (keys == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "keys is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    keys->clear();
    try {
      keys->reserve(set_.size());
      for (typename SetType::const_iterator it = set_.begin(); it != set_.end(); ++it) {
        keys->push_back(*it);
      }
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "snapshot allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "snapshot failed");
    }
    return api::Status::Ok();
  }

  virtual std::size_t ApproxSize() const {
    std::lock_guard<std::mutex> lock(mu_);
    return set_.size();
  }

 private:
  typedef corekit::memory::GlobalStlAllocator<K> KeyAlloc;
  typedef std::unordered_set<K, std::hash<K>, std::equal_to<K>, KeyAlloc> SetType;

  mutable std::mutex mu_;
  SetType set_;
};

template <typename K>
using BasicConcurrentSet = BasicConcurrentSetImpl<K>;

#undef CK_STATUS

}  // namespace concurrent
}  // namespace corekit

