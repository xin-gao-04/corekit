#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "corekit/concurrent/i_map.hpp"

namespace corekit {
namespace concurrent {

template <typename K, typename V>
class BasicConcurrentMap : public IConcurrentMap<K, V> {
 public:
  BasicConcurrentMap() {}
  virtual ~BasicConcurrentMap() {}

  virtual const char* Name() const { return "corekit.concurrent.basic_map"; }
  virtual std::uint32_t ApiVersion() const { return api::kApiVersion; }
  virtual void Release() { delete this; }

  virtual api::Status Upsert(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mu_);
    map_[key] = value;
    return api::Status::Ok();
  }

  virtual api::Status InsertIfAbsent(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (map_.find(key) != map_.end()) {
      return api::Status(api::StatusCode::kWouldBlock, "key already exists");
    }
    map_[key] = value;
    return api::Status::Ok();
  }

  virtual api::Result<V> Find(const K& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    typename std::unordered_map<K, V>::const_iterator it = map_.find(key);
    if (it == map_.end()) {
      return api::Result<V>(api::Status(api::StatusCode::kNotFound, "key not found"));
    }
    return api::Result<V>(it->second);
  }

  virtual api::Status TryGet(const K& key, V* out) const {
    if (out == NULL) {
      return api::Status(api::StatusCode::kInvalidArgument, "out is null");
    }
    std::lock_guard<std::mutex> lock(mu_);
    typename std::unordered_map<K, V>::const_iterator it = map_.find(key);
    if (it == map_.end()) {
      return api::Status(api::StatusCode::kNotFound, "key not found");
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
    typename std::unordered_map<K, V>::iterator it = map_.find(key);
    if (it == map_.end()) {
      return api::Status(api::StatusCode::kNotFound, "key not found");
    }
    map_.erase(it);
    return api::Status::Ok();
  }

  virtual api::Status Clear() {
    std::lock_guard<std::mutex> lock(mu_);
    map_.clear();
    return api::Status::Ok();
  }

  virtual std::size_t ApproxSize() const {
    std::lock_guard<std::mutex> lock(mu_);
    return map_.size();
  }

 private:
  mutable std::mutex mu_;
  std::unordered_map<K, V> map_;
};

}  // namespace concurrent
}  // namespace corekit
