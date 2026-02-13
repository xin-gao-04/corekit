#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <new>

#include "corekit/memory/i_global_allocator.hpp"

namespace corekit {
namespace memory {

template <typename T>
class GlobalStlAllocator {
 public:
  typedef T value_type;

  GlobalStlAllocator() noexcept {}
  template <typename U>
  GlobalStlAllocator(const GlobalStlAllocator<U>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n == 0) return NULL;
    if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
      throw std::bad_alloc();
    }
    const std::size_t bytes = n * sizeof(T);
    const std::size_t alignment = alignof(T) < sizeof(void*) ? sizeof(void*) : alignof(T);
    api::Result<void*> r = GlobalAllocator::Allocate(bytes, alignment);
    if (!r.ok() || r.value() == NULL) {
      throw std::bad_alloc();
    }
    return static_cast<T*>(r.value());
  }

  void deallocate(T* p, std::size_t) noexcept {
    GlobalAllocator::Deallocate(static_cast<void*>(p));
  }

  template <typename U>
  struct rebind {
    typedef GlobalStlAllocator<U> other;
  };
};

template <typename T, typename U>
bool operator==(const GlobalStlAllocator<T>&, const GlobalStlAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const GlobalStlAllocator<T>&, const GlobalStlAllocator<U>&) noexcept {
  return false;
}

}  // namespace memory
}  // namespace corekit
