#include "memory/system_allocator.hpp"

#include <cstdlib>

#include "corekit/api/version.hpp"

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace corekit {
namespace memory {
namespace {

bool IsPowerOfTwo(std::size_t x) { return x != 0 && (x & (x - 1)) == 0; }

}  // namespace

SystemAllocator::SystemAllocator() : backend_(AllocBackend::kSystem) {}
SystemAllocator::~SystemAllocator() {}

const char* SystemAllocator::Name() const { return "corekit.memory.system_allocator"; }
std::uint32_t SystemAllocator::ApiVersion() const { return api::kApiVersion; }
void SystemAllocator::Release() { delete this; }

api::Status SystemAllocator::SetBackend(AllocBackend backend) {
  if (backend != AllocBackend::kSystem) {
    return api::Status(api::StatusCode::kUnsupported,
                       "Only kSystem backend is implemented in current stage");
  }
  backend_ = backend;
  return api::Status::Ok();
}

api::Result<void*> SystemAllocator::Allocate(std::size_t size, std::size_t alignment) {
  if (backend_ != AllocBackend::kSystem) {
    return api::Result<void*>(api::Status(api::StatusCode::kUnsupported,
                                          "Selected backend is not implemented"));
  }
  if (size == 0) {
    return api::Result<void*>(
        api::Status(api::StatusCode::kInvalidArgument, "size must be > 0"));
  }
  if (alignment < sizeof(void*) || !IsPowerOfTwo(alignment)) {
    return api::Result<void*>(api::Status(api::StatusCode::kInvalidArgument,
                                          "alignment must be power-of-two and >= sizeof(void*)"));
  }

#if defined(_WIN32)
  void* ptr = _aligned_malloc(size, alignment);
  if (ptr == NULL) {
    return api::Result<void*>(api::Status(api::StatusCode::kInternalError,
                                          "_aligned_malloc failed"));
  }
#else
  void* ptr = NULL;
  if (posix_memalign(&ptr, alignment, size) != 0 || ptr == NULL) {
    return api::Result<void*>(api::Status(api::StatusCode::kInternalError,
                                          "posix_memalign failed"));
  }
#endif
  return api::Result<void*>(ptr);
}

api::Status SystemAllocator::Deallocate(void* ptr) {
  if (ptr == NULL) {
    return api::Status::Ok();
  }
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
  return api::Status::Ok();
}

}  // namespace memory
}  // namespace corekit


