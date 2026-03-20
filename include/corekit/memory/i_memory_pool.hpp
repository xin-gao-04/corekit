#pragma once

#include <cstddef>
#include <cstdint>

#include "corekit/api/status.hpp"

namespace corekit {
namespace memory {

/// Memory pool statistics.
struct PoolStats {
  std::uint64_t alloc_count = 0;       // Total allocation requests.
  std::uint64_t free_count = 0;        // Total free requests.
  std::uint64_t bytes_in_use = 0;      // Currently allocated bytes.
  std::uint64_t bytes_peak = 0;        // High-water mark.
  std::uint64_t bytes_limit = 0;       // Budget limit (0 = unlimited).
  std::uint64_t block_count = 0;       // Number of large blocks held.
  std::uint64_t slab_hits = 0;         // Allocations served from slab cache.
  std::uint64_t slab_misses = 0;       // Allocations that needed a new block.
};

/// Configuration for a memory pool.
struct PoolConfig {
  const char* name = "default";        // Pool name for diagnostics.
  std::size_t block_size = 65536;      // Block size in bytes (default 64KB).
  std::size_t max_bytes = 0;           // Memory limit (0 = unlimited).
  std::size_t max_cached_blocks = 64;  // Max idle blocks to retain.
  bool enable_guard_bytes = false;     // Add guard patterns for overflow detection.
  bool thread_safe = true;             // Enable locking.
};

/// Hierarchical memory pool with slab-based sub-allocation.
///
/// Follows the OSHeap pattern: the pool acquires large blocks from the
/// underlying allocator and subdivides them into fixed-size slabs. Small
/// allocations are served from the slab cache; large allocations go
/// directly to the underlying allocator.
///
/// Supports child pools for subsystem-level budget isolation.
///
/// Usage:
///   auto* pool = corekit_create_memory_pool();
///   PoolConfig cfg;
///   cfg.name = "physics";
///   cfg.max_bytes = 64 * 1024 * 1024; // 64MB budget
///   pool->Init(cfg);
///
///   void* p = pool->Alloc(128);
///   pool->Free(p);
class IMemoryPool {
 public:
  virtual ~IMemoryPool() {}

  virtual const char* Name() const = 0;
  virtual void Release() = 0;

  /// Initialize the pool with the given configuration.
  virtual api::Status Init(const PoolConfig& config) = 0;

  /// Allocate memory from the pool.
  /// Small sizes (<=4096) are served from slab cache.
  /// Larger sizes go directly to the underlying allocator.
  virtual void* Alloc(std::size_t size) = 0;

  /// Allocate aligned memory from the pool.
  virtual void* AllocAligned(std::size_t size, std::size_t alignment) = 0;

  /// Free memory previously allocated from this pool.
  virtual void Free(void* ptr) = 0;

  /// Create a child pool with its own budget, backed by this pool's blocks.
  /// The child pool's memory usage counts against the parent's limit too.
  virtual api::Result<IMemoryPool*> CreateChild(const PoolConfig& config) = 0;

  /// Get current pool statistics.
  virtual PoolStats Stats() const = 0;

  /// Release all cached (idle) blocks back to the underlying allocator.
  /// Does not affect currently allocated memory.
  virtual api::Status Shrink() = 0;

  /// Check if a pointer was allocated from this pool.
  virtual bool Owns(const void* ptr) const = 0;
};

// Pool-aware placement new / delete helpers (C++14 compatible).
template <typename T>
T* PoolNew(IMemoryPool* pool) {
  void* raw = pool->Alloc(sizeof(T));
  if (raw == NULL) return NULL;
  try {
    return new (raw) T();
  } catch (...) {
    pool->Free(raw);
    return NULL;
  }
}

template <typename T, typename A1>
T* PoolNew(IMemoryPool* pool, A1&& a1) {
  void* raw = pool->Alloc(sizeof(T));
  if (raw == NULL) return NULL;
  try {
    return new (raw) T(static_cast<A1&&>(a1));
  } catch (...) {
    pool->Free(raw);
    return NULL;
  }
}

template <typename T, typename A1, typename A2>
T* PoolNew(IMemoryPool* pool, A1&& a1, A2&& a2) {
  void* raw = pool->Alloc(sizeof(T));
  if (raw == NULL) return NULL;
  try {
    return new (raw) T(static_cast<A1&&>(a1), static_cast<A2&&>(a2));
  } catch (...) {
    pool->Free(raw);
    return NULL;
  }
}

template <typename T, typename A1, typename A2, typename A3>
T* PoolNew(IMemoryPool* pool, A1&& a1, A2&& a2, A3&& a3) {
  void* raw = pool->Alloc(sizeof(T));
  if (raw == NULL) return NULL;
  try {
    return new (raw) T(static_cast<A1&&>(a1), static_cast<A2&&>(a2), static_cast<A3&&>(a3));
  } catch (...) {
    pool->Free(raw);
    return NULL;
  }
}

template <typename T>
void PoolDelete(IMemoryPool* pool, T* ptr) {
  if (ptr == NULL) return;
  ptr->~T();
  pool->Free(static_cast<void*>(ptr));
}

}  // namespace memory
}  // namespace corekit

// Pool-aware allocation macros (take pool as first argument).
#define COREKIT_POOL_ALLOC(pool, bytes) ((pool)->Alloc((bytes)))

#define COREKIT_POOL_ALLOC_ALIGNED(pool, bytes, alignment) \
  ((pool)->AllocAligned((bytes), (alignment)))

#define COREKIT_POOL_FREE(pool, ptr) ((pool)->Free((ptr)))

#define COREKIT_POOL_NEW(pool, Type, ...) \
  ::corekit::memory::PoolNew<Type>((pool), ##__VA_ARGS__)

#define COREKIT_POOL_DELETE(pool, ptr) \
  ::corekit::memory::PoolDelete((pool), (ptr))
