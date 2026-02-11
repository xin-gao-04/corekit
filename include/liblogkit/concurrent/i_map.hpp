#pragma once

#include <cstddef>
#include <cstdint>

#include "liblogkit/api/status.hpp"
#include "liblogkit/api/version.hpp"

namespace liblogkit {
namespace concurrent {

template <typename K, typename V>
class IConcurrentMap {
 public:
  virtual ~IConcurrentMap() {}

  // 返回实现名称。
  virtual const char* Name() const = 0;

  // 返回当前对象遵循的接口版本。
  virtual std::uint32_t ApiVersion() const = 0;

  // 释放实例对象本身。调用后对象失效。
  virtual void Release() = 0;

  // 插入或更新键值。
  // 返回：kOk 表示写入成功。
  virtual api::Status Upsert(const K& key, const V& value) = 0;

  // 查询键值。
  // 返回：
  // - kOk：value 为查询结果。
  // - kNotFound：不存在该 key。
  virtual api::Result<V> Find(const K& key) const = 0;

  // 删除键。
  // 返回：kOk 表示删除成功；kNotFound 表示键不存在。
  virtual api::Status Erase(const K& key) = 0;

  // 返回近似元素个数。
  virtual std::size_t ApproxSize() const = 0;
};

}  // namespace concurrent
}  // namespace liblogkit
