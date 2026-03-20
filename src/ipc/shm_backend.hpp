#pragma once

#include <cstddef>
#include <string>

#include "corekit/api/status.hpp"

namespace corekit {
namespace ipc {

/// Platform abstraction for shared memory operations.
/// Implementations: Win32 (CreateFileMapping) and POSIX (shm_open + mmap).
class IShmBackend {
 public:
  virtual ~IShmBackend() {}

  /// Create a new shared memory region (server role).
  /// Returns kAlreadyInitialized if the name already exists.
  virtual api::Status Create(const std::string& name, std::size_t size) = 0;

  /// Open an existing shared memory region (client role).
  /// Returns kNotFound if the region does not exist.
  virtual api::Status Open(const std::string& name, std::size_t min_size) = 0;

  /// Base address of the mapped region, or NULL if not mapped.
  virtual void* BaseAddress() const = 0;

  /// Size of the mapped region.
  virtual std::size_t MappedSize() const = 0;

  /// Unmap and close handles. Safe to call multiple times.
  virtual void Close() = 0;

  /// Whether the backend is currently mapped.
  virtual bool IsOpen() const = 0;
};

/// Create the platform-appropriate shared memory backend.
IShmBackend* CreateShmBackend();

}  // namespace ipc
}  // namespace corekit
