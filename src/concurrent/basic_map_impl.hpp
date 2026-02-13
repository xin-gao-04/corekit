#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "corekit/api/version.hpp"
#include "corekit/concurrent/i_map.hpp"
#include "src/memory/global_stl_allocator.hpp"

namespace corekit {
namespace concurrent {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kConcurrent)

template <typename K, typename V>
class BasicConcurrentMapImpl : public IConcurrentMap<K, V> {
 public:
  BasicConcurrentMapImpl() {}
  virtual ~BasicConcurrentMapImpl() {}

  virtual const char* Name() const { return "corekit.concurrent.basic_map"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status Upsert(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mu_);
    try {
      map_[key] = value;
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "map allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "map upsert failed");
    }
    return api::Status::Ok();
  }

  virtual api::Status InsertOrAssign(const K& key, const V& value, bool* inserted) {
    std::lock_guard<std::mutex> lock(mu_);
    try {
      typename MapType::iterator it = map_.find(key);
      if (it == map_.end()) {
        map_[key] = value;
        if (inserted != NULL) *inserted = true;
        return api::Status::Ok();
      }
      it->second = value;
      if (inserted != NULL) *inserted = false;
      return api::Status::Ok();
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "map allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "map insert-or-assign failed");
    }
  }

  virtual api::Status InsertIfAbsent(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (map_.find(key) != map_.end()) {
      return CK_STATUS(api::StatusCode::kWouldBlock, "key already exists");
    }
    try {
      map_[key] = value;
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "map allocation failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "map insert failed");
    }
    return api::Status::Ok();
  }

  virtual api::Result<V> Find(const K& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    typename MapType::const_iterator it = map_.find(key);
    if (it == map_.end()) {
      return api::Result<V>(CK_STATUS(api::StatusCode::kNotFound, "key not found"));
    }
    return api::Result<V>(it->second);
  }

  virtual api::Status TryGet(const K& key, V* out) const {
    if (out == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "out is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    typename MapType::const_iterator it = map_.find(key);
    if (it == map_.end()) {
      return CK_STATUS(api::StatusCode::kNotFound, "key not found");
    }
    *out = it->second;
    return api::Status::Ok();
  }

  virtual bool Contains(const K& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    return map_.find(key) != map_.end();
  }

  virtual api::Status Erase(const K& key) {
    std::lock_guard<std::mutex> lock(mu_);
    typename MapType::iterator it = map_.find(key);
    if (it == map_.end()) {
      return CK_STATUS(api::StatusCode::kNotFound, "key not found");
    }
    map_.erase(it);
    return api::Status::Ok();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    map_.clear();
    return api::Status::Ok();
  }

  virtual api::Status Reserve(std::size_t expected_size) {
    std::lock_guard<std::mutex> lock(mu_);
    try {
      map_.reserve(expected_size);
    } catch (const std::bad_alloc&) {
      return CK_STATUS(api::StatusCode::kInternalError, "map reserve failed");
    } catch (...) {
      return CK_STATUS(api::StatusCode::kInternalError, "map reserve failed");
    }
    return api::Status::Ok();
  }

  virtual api::Status SnapshotKeys(std::vector<K>* keys) const {
    if (keys == NULL) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "keys is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    keys->clear();
    try {
      keys->reserve(map_.size());
      for (typename MapType::const_iterator it = map_.begin(); it != map_.end(); ++it) {
        keys->push_back(it->first);
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
    return map_.size();
  }

 private:
  typedef std::pair<const K, V> PairType;
  typedef corekit::memory::GlobalStlAllocator<PairType> PairAlloc;
  typedef std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, PairAlloc> MapType;

  mutable std::mutex mu_;
  MapType map_;
};

template <typename K, typename V>
using BasicConcurrentMap = BasicConcurrentMapImpl<K, V>;

#undef CK_STATUS

}  // namespace concurrent
}  // namespace corekit

