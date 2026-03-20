#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "corekit/api/export.hpp"
#include "corekit/api/status.hpp"

namespace corekit {
namespace io {

/// File open mode.
enum class FileMode {
  kRead = 0,       // Open for reading (file must exist).
  kWrite,          // Open for writing (creates/truncates).
  kAppend,         // Open for appending (creates if needed).
  kReadWrite,      // Open for both reading and writing (file must exist).
};

/// Seek origin.
enum class SeekOrigin {
  kBegin = 0,
  kCurrent,
  kEnd,
};

/// File interface — platform-independent binary file I/O.
///
/// Usage:
///   auto* f = corekit_create_file();
///   f->Open("data.bin", FileMode::kWrite);
///   f->Write(buf, 1024);
///   f->Close();
///   f->Release();
class IFile {
 public:
  virtual ~IFile() {}

  virtual const char* Name() const = 0;
  virtual void Release() = 0;

  /// Open a file at the given path with the specified mode.
  virtual api::Status Open(const char* path, FileMode mode) = 0;

  /// Read up to `bytes` from the file. Returns the number of bytes actually read.
  virtual api::Result<std::size_t> Read(void* buf, std::size_t bytes) = 0;

  /// Write `bytes` to the file. Returns the number of bytes actually written.
  virtual api::Result<std::size_t> Write(const void* buf, std::size_t bytes) = 0;

  /// Seek to offset relative to origin.
  virtual api::Status Seek(std::int64_t offset, SeekOrigin origin) = 0;

  /// Get current file position.
  virtual api::Result<std::int64_t> Tell() = 0;

  /// Flush pending writes.
  virtual api::Status Flush() = 0;

  /// Close the file.
  virtual api::Status Close() = 0;

  /// Check if the file is currently open.
  virtual bool IsOpen() const = 0;
};

/// Static file utility functions.
class COREKIT_API FileUtils {
 public:
  /// Check if a file exists.
  static bool Exists(const char* path);

  /// Get file size in bytes.
  static api::Result<std::uint64_t> FileSize(const char* path);

  /// Delete a file.
  static api::Status Delete(const char* path);

  /// Read entire file contents into a byte vector.
  static api::Result<std::vector<std::uint8_t> > ReadAll(const char* path);

  /// Write data to a file (creates/truncates).
  static api::Status WriteAll(const char* path, const void* data, std::size_t size);
};

}  // namespace io
}  // namespace corekit
