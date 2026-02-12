#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "corekit/api/status.hpp"
#include "corekit/api/version.hpp"

namespace corekit {
namespace concurrent {

template <typename K>
class IConcurrentSet {
 public:
  virtual ~IConcurrentSet() {}

  // 返回实现名称。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 插入键。
  // 返回：
  // - kOk：插入成功。
  // - kWouldBlock：键已存在。
  virtual api::Status Insert(const K& key) = 0;

  // 删除键。
  // 返回：kOk 表示删除成功；kNotFound 表示键不存在。
  virtual api::Status Erase(const K& key) = 0;

  // 判断 key 是否存在。
  virtual bool Contains(const K& key) const = 0;

  // 清空集合。
  virtual api::Status Clear() = 0;

  // 预留容量，减少重哈希。
  virtual api::Status Reserve(std::size_t expected_size) = 0;

  // 导出 key 快照。
  virtual api::Status Snapshot(std::vector<K>* keys) const = 0;

  // 返回近似元素个数。
  virtual std::size_t ApproxSize() const = 0;
};

}  // namespace concurrent
}  // namespace corekit
