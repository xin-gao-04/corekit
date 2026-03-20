#pragma once

#include <cstdio>

#include "corekit/io/i_file.hpp"

namespace corekit {
namespace io {

/// Standard C FILE* based implementation of IFile.
/// Portable across all platforms (Win32, Linux, macOS).
class StdFile : public IFile {
 public:
  StdFile() : fp_(NULL) {}

  ~StdFile() {
    if (fp_ != NULL) {
      std::fclose(fp_);
      fp_ = NULL;
    }
  }

  const char* Name() const override { return "StdFile"; }
  void Release() override { delete this; }

  api::Status Open(const char* path, FileMode mode) override {
    if (fp_ != NULL) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "file already open");
    }
    if (path == NULL || path[0] == '\0') {
      return api::Status(api::StatusCode::kInvalidArgument, "path is empty");
    }

    const char* fmode = NULL;
    switch (mode) {
      case FileMode::kRead:      fmode = "rb";  break;
      case FileMode::kWrite:     fmode = "wb";  break;
      case FileMode::kAppend:    fmode = "ab";  break;
      case FileMode::kReadWrite: fmode = "r+b"; break;
      default:
        return api::Status(api::StatusCode::kInvalidArgument, "invalid file mode");
    }

    fp_ = std::fopen(path, fmode);
    if (fp_ == NULL) {
      return api::Status(api::StatusCode::kIoError, "failed to open file");
    }
    return api::Status::Ok();
  }

  api::Result<std::size_t> Read(void* buf, std::size_t bytes) override {
    if (fp_ == NULL) {
      return api::Result<std::size_t>(
          api::Status(api::StatusCode::kNotInitialized, "file not open"));
    }
    if (buf == NULL || bytes == 0) {
      return api::Result<std::size_t>(static_cast<std::size_t>(0));
    }
    std::size_t n = std::fread(buf, 1, bytes, fp_);
    if (n == 0 && std::ferror(fp_)) {
      return api::Result<std::size_t>(
          api::Status(api::StatusCode::kIoError, "read failed"));
    }
    return api::Result<std::size_t>(n);
  }

  api::Result<std::size_t> Write(const void* buf, std::size_t bytes) override {
    if (fp_ == NULL) {
      return api::Result<std::size_t>(
          api::Status(api::StatusCode::kNotInitialized, "file not open"));
    }
    if (buf == NULL || bytes == 0) {
      return api::Result<std::size_t>(static_cast<std::size_t>(0));
    }
    std::size_t n = std::fwrite(buf, 1, bytes, fp_);
    if (n < bytes && std::ferror(fp_)) {
      return api::Result<std::size_t>(
          api::Status(api::StatusCode::kIoError, "write failed"));
    }
    return api::Result<std::size_t>(n);
  }

  api::Status Seek(std::int64_t offset, SeekOrigin origin) override {
    if (fp_ == NULL) {
      return api::Status(api::StatusCode::kNotInitialized, "file not open");
    }
    int whence = SEEK_SET;
    switch (origin) {
      case SeekOrigin::kBegin:   whence = SEEK_SET; break;
      case SeekOrigin::kCurrent: whence = SEEK_CUR; break;
      case SeekOrigin::kEnd:     whence = SEEK_END; break;
    }
#if defined(_WIN32)
    int rc = _fseeki64(fp_, offset, whence);
#else
    int rc = std::fseek(fp_, static_cast<long>(offset), whence);
#endif
    if (rc != 0) {
      return api::Status(api::StatusCode::kIoError, "seek failed");
    }
    return api::Status::Ok();
  }

  api::Result<std::int64_t> Tell() override {
    if (fp_ == NULL) {
      return api::Result<std::int64_t>(
          api::Status(api::StatusCode::kNotInitialized, "file not open"));
    }
#if defined(_WIN32)
    std::int64_t pos = _ftelli64(fp_);
#else
    std::int64_t pos = static_cast<std::int64_t>(std::ftell(fp_));
#endif
    if (pos < 0) {
      return api::Result<std::int64_t>(
          api::Status(api::StatusCode::kIoError, "tell failed"));
    }
    return api::Result<std::int64_t>(pos);
  }

  api::Status Flush() override {
    if (fp_ == NULL) {
      return api::Status(api::StatusCode::kNotInitialized, "file not open");
    }
    if (std::fflush(fp_) != 0) {
      return api::Status(api::StatusCode::kIoError, "flush failed");
    }
    return api::Status::Ok();
  }

  api::Status Close() override {
    if (fp_ == NULL) {
      return api::Status::Ok();
    }
    int rc = std::fclose(fp_);
    fp_ = NULL;
    if (rc != 0) {
      return api::Status(api::StatusCode::kIoError, "close failed");
    }
    return api::Status::Ok();
  }

  bool IsOpen() const override { return fp_ != NULL; }

 private:
  std::FILE* fp_;
};

}  // namespace io
}  // namespace corekit
