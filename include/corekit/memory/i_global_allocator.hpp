#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "corekit/api/export.hpp"
#include "corekit/memory/iallocator.hpp"

namespace corekit {
namespace memory {

struct GlobalAllocatorOptions {
  AllocBackend backend = AllocBackend::kSystem;
  bool strict_backend = true;
};

class COREKIT_API GlobalAllocator {
 public:
  // Configure global allocator policy explicitly.
  static api::Status Configure(const GlobalAllocatorOptions& options);

  // Load allocator policy from JSON config file.
  // Supported schema:
  // {
  //   "memory": {
  //     "backend": "system|tbb|mimalloc",
  //     "strict_backend": true|false
  //   }
  // }
  static api::Status ConfigureFromFile(const std::string& config_path);

  // Allocate/deallocate through global allocator.
  static api::Result<void*> Allocate(std::size_t size, std::size_t alignment);
  static api::Status Deallocate(void* ptr);

  // Snapshot current backend setting.
  static AllocBackend CurrentBackend();

  // Backend metadata/introspection.
  static const char* BackendDisplayName(AllocBackend backend);
  static bool IsBackendEnabled(AllocBackend backend);

  // Runtime observability helpers.
  static const char* CurrentBackendName();
  static AllocatorCaps CurrentCaps();
  static AllocatorStats CurrentStats();
  static void ResetCurrentStats();
};

inline void* GlobalAllocOrNull(std::size_t size, std::size_t alignment) {
  const std::size_t normalized = alignment < sizeof(void*) ? sizeof(void*) : alignment;
  api::Result<void*> r = GlobalAllocator::Allocate(size, normalized);
  return r.ok() ? r.value() : NULL;
}

inline void GlobalFreeIgnore(void* ptr) {
  (void)GlobalAllocator::Deallocate(ptr);
}

template <typename T, typename... Args>
T* GlobalNew(Args&&... args) {
  void* raw = GlobalAllocOrNull(sizeof(T), alignof(T));
  if (raw == NULL) return NULL;
  try {
    return new (raw) T(std::forward<Args>(args)...);
  } catch (...) {
    GlobalFreeIgnore(raw);
    return NULL;
  }
}

template <typename T>
void GlobalDelete(T* ptr) {
  if (ptr == NULL) return;
  ptr->~T();
  GlobalFreeIgnore(static_cast<void*>(ptr));
}

}  // namespace memory
}  // namespace corekit

#define COREKIT_ALLOC(bytes) \
  ::corekit::memory::GlobalAllocOrNull((bytes), alignof(std::max_align_t))

#define COREKIT_ALLOC_ALIGNED(bytes, alignment) \
  ::corekit::memory::GlobalAllocOrNull((bytes), (alignment))

#define COREKIT_FREE(ptr) ::corekit::memory::GlobalFreeIgnore((ptr))

#define COREKIT_NEW(Type, ...) ::corekit::memory::GlobalNew<Type>(__VA_ARGS__)

#define COREKIT_DELETE(ptr) ::corekit::memory::GlobalDelete((ptr))

