#include "memory/mimalloc_allocator.hpp"

#include "corekit/api/version.hpp"

#include <mimalloc.h>

namespace corekit {
namespace memory {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kMemory)
namespace {

bool IsPowerOfTwo(std::size_t x) { return x != 0 && (x & (x - 1)) == 0; }

}  // namespace

MimallocAllocator::MimallocAllocator()
    : alloc_count_(0), free_count_(0), alloc_fail_count_(0), bytes_in_use_(0), bytes_peak_(0) {}
MimallocAllocator::~MimallocAllocator() {}

const char* MimallocAllocator::Name() const { return "corekit.memory.mimalloc_allocator"; }
const char* MimallocAllocator::BackendName() const { return "mimalloc"; }
std::uint32_t MimallocAllocator::ApiVersion() const { return api::kApiVersion; }
void MimallocAllocator::Release() { delete this; }

AllocatorCaps MimallocAllocator::Caps() const {
  AllocatorCaps caps;
  caps.supports_aligned_alloc = true;
  caps.supports_runtime_switch = false;
  caps.thread_safe = true;
  return caps;
}

AllocatorStats MimallocAllocator::Stats() const {
  AllocatorStats s;
  s.alloc_count = alloc_count_.load(std::memory_order_relaxed);
  s.free_count = free_count_.load(std::memory_order_relaxed);
  s.alloc_fail_count = alloc_fail_count_.load(std::memory_order_relaxed);
  s.bytes_in_use = bytes_in_use_.load(std::memory_order_relaxed);
  s.bytes_peak = bytes_peak_.load(std::memory_order_relaxed);
  return s;
}

void MimallocAllocator::ResetStats() {
  std::uint64_t live = 0;
  {
    std::lock_guard<std::mutex> lock(size_mu_);
    for (std::unordered_map<void*, std::size_t>::const_iterator it = alloc_sizes_.begin();
         it != alloc_sizes_.end(); ++it) {
      live += static_cast<std::uint64_t>(it->second);
    }
  }
  alloc_count_.store(0, std::memory_order_relaxed);
  free_count_.store(0, std::memory_order_relaxed);
  alloc_fail_count_.store(0, std::memory_order_relaxed);
  bytes_in_use_.store(live, std::memory_order_relaxed);
  bytes_peak_.store(live, std::memory_order_relaxed);
}

void MimallocAllocator::RecordAllocFailure() {
  alloc_fail_count_.fetch_add(1, std::memory_order_relaxed);
}

void MimallocAllocator::RecordAllocSuccess(void* ptr, std::size_t size) {
  alloc_count_.fetch_add(1, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(size_mu_);
    alloc_sizes_[ptr] = size;
  }
  const std::uint64_t in_use_now =
      bytes_in_use_.fetch_add(static_cast<std::uint64_t>(size), std::memory_order_relaxed) +
      static_cast<std::uint64_t>(size);

  std::uint64_t peak = bytes_peak_.load(std::memory_order_relaxed);
  while (in_use_now > peak &&
         !bytes_peak_.compare_exchange_weak(peak, in_use_now, std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
  }
}

void MimallocAllocator::RecordDeallocate(void* ptr) {
  std::size_t released = 0;
  {
    std::lock_guard<std::mutex> lock(size_mu_);
    std::unordered_map<void*, std::size_t>::iterator it = alloc_sizes_.find(ptr);
    if (it != alloc_sizes_.end()) {
      released = it->second;
      alloc_sizes_.erase(it);
    }
  }

  free_count_.fetch_add(1, std::memory_order_relaxed);
  if (released > 0) {
    const std::uint64_t delta = static_cast<std::uint64_t>(released);
    std::uint64_t cur = bytes_in_use_.load(std::memory_order_relaxed);
    while (true) {
      const std::uint64_t next = cur > delta ? (cur - delta) : 0;
      if (bytes_in_use_.compare_exchange_weak(cur, next, std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
        break;
      }
    }
  }
}

api::Status MimallocAllocator::SetBackend(AllocBackend backend) {
  if (backend != AllocBackend::kMimalloc) {
    return CK_STATUS(api::StatusCode::kUnsupported,
                     "MimallocAllocator only supports kMimalloc backend");
  }
  return api::Status::Ok();
}

api::Result<void*> MimallocAllocator::Allocate(std::size_t size, std::size_t alignment) {
  if (size == 0) {
    RecordAllocFailure();
    return api::Result<void*>(CK_STATUS(api::StatusCode::kInvalidArgument, "size must be > 0"));
  }
  if (alignment < sizeof(void*) || !IsPowerOfTwo(alignment)) {
    RecordAllocFailure();
    return api::Result<void*>(CK_STATUS(api::StatusCode::kInvalidArgument,
                                        "alignment must be power-of-two and >= sizeof(void*)"));
  }

  void* ptr = mi_malloc_aligned(size, alignment);
  if (ptr == NULL) {
    RecordAllocFailure();
    return api::Result<void*>(CK_STATUS(api::StatusCode::kInternalError, "mi_malloc_aligned failed"));
  }
  RecordAllocSuccess(ptr, size);
  return api::Result<void*>(ptr);
}

api::Status MimallocAllocator::Deallocate(void* ptr) {
  if (ptr == NULL) return api::Status::Ok();
  RecordDeallocate(ptr);
  mi_free(ptr);
  return api::Status::Ok();
}

#undef CK_STATUS

}  // namespace memory
}  // namespace corekit
