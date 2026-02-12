#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_set.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

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
      return api::Status(api::StatusCode::kWouldBlock, "key already exists");
    }
    set_.insert(key);
    return api::Status::Ok();
  }

  virtual api::Status Erase(const K& key) {
    std::lock_guard<std::mutex> lock(mu_);
    typename SetType::iterator it = set_.find(key);
    if (it == set_.end()) {
      return api::Status(api::StatusCode::kNotFound, "key not found");
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
    set_.reserve(expected_size);
    return api::Status::Ok();
  }

  virtual api::Status Snapshot(std::vector<K>* keys) const {
    if (keys == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "keys is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    keys->clear();
    keys->reserve(set_.size());
    for (typename SetType::const_iterator it = set_.begin(); it != set_.end(); ++it) {
      keys->push_back(*it);
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

}  // namespace concurrent
}  // namespace corekit
