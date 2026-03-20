#pragma once

#include <string>

#include "corekit/api/export.hpp"
#include "corekit/api/status.hpp"
#include "corekit/memory/i_memory_pool.hpp"

namespace corekit {
namespace memory {

/// Static global system memory pool — the root pool for the entire process.
///
/// Follows the same singleton pattern as GlobalAllocator. The system pool is
/// lazily initialized with default config on first access. Users can call
/// Configure() or ConfigureFromFile() before first use to customize.
///
/// Usage:
///   // Use default config:
///   void* p = SystemPool::Instance()->Alloc(128);
///   SystemPool::Instance()->Free(p);
///
///   // Or configure from JSON:
///   SystemPool::ConfigureFromFile("config/corekit.json");
///   auto* child = SystemPool::Instance()->CreateChild(childCfg).value();
class COREKIT_API SystemPool {
 public:
  /// Configure the system pool before first use.
  /// Returns kAlreadyInitialized if pool is already created and in use.
  static api::Status Configure(const PoolConfig& config);

  /// Load pool configuration from JSON config file.
  /// Reads the "pool" section of the config file:
  /// {
  ///   "pool": {
  ///     "name": "system",
  ///     "block_size": 65536,
  ///     "max_bytes": 0,
  ///     "max_cached_blocks": 64,
  ///     "enable_guard_bytes": false,
  ///     "thread_safe": true
  ///   }
  /// }
  static api::Status ConfigureFromFile(const std::string& config_path);

  /// Get the singleton system memory pool instance.
  /// Creates and initializes lazily on first call.
  static IMemoryPool* Instance();

  /// Get current stats of the system pool.
  static PoolStats CurrentStats();

  /// Shrink the system pool (release idle blocks).
  static api::Status Shrink();
};

}  // namespace memory
}  // namespace corekit

// Convenience macros using the system pool singleton.
#define COREKIT_SYS_ALLOC(bytes) \
  (::corekit::memory::SystemPool::Instance()->Alloc((bytes)))

#define COREKIT_SYS_ALLOC_ALIGNED(bytes, alignment) \
  (::corekit::memory::SystemPool::Instance()->AllocAligned((bytes), (alignment)))

#define COREKIT_SYS_FREE(ptr) \
  (::corekit::memory::SystemPool::Instance()->Free((ptr)))

#define COREKIT_SYS_NEW(Type, ...) \
  ::corekit::memory::PoolNew<Type>(::corekit::memory::SystemPool::Instance(), ##__VA_ARGS__)

#define COREKIT_SYS_DELETE(ptr) \
  ::corekit::memory::PoolDelete(::corekit::memory::SystemPool::Instance(), (ptr))
