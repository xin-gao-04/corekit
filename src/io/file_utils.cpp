#include "corekit/io/i_file.hpp"

#include <cstdio>
#include <sys/stat.h>

#if defined(_WIN32)
#include <io.h>
#define ck_stat _stat64
#define ck_stat_t struct __stat64
#else
#define ck_stat stat
#define ck_stat_t struct stat
#endif

namespace corekit {
namespace io {

bool FileUtils::Exists(const char* path) {
  if (path == NULL || path[0] == '\0') return false;
  ck_stat_t st;
  return ck_stat(path, &st) == 0;
}

api::Result<std::uint64_t> FileUtils::FileSize(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return api::Result<std::uint64_t>(
        api::Status(api::StatusCode::kInvalidArgument, "path is empty"));
  }
  ck_stat_t st;
  if (ck_stat(path, &st) != 0) {
    return api::Result<std::uint64_t>(
        api::Status(api::StatusCode::kNotFound, "file not found"));
  }
  return api::Result<std::uint64_t>(static_cast<std::uint64_t>(st.st_size));
}

api::Status FileUtils::Delete(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return api::Status(api::StatusCode::kInvalidArgument, "path is empty");
  }
  if (std::remove(path) != 0) {
    return api::Status(api::StatusCode::kIoError, "failed to delete file");
  }
  return api::Status::Ok();
}

api::Result<std::vector<std::uint8_t> > FileUtils::ReadAll(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return api::Result<std::vector<std::uint8_t> >(
        api::Status(api::StatusCode::kInvalidArgument, "path is empty"));
  }

  std::FILE* fp = std::fopen(path, "rb");
  if (fp == NULL) {
    return api::Result<std::vector<std::uint8_t> >(
        api::Status(api::StatusCode::kNotFound, "file not found"));
  }

  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);

  if (sz < 0) {
    std::fclose(fp);
    return api::Result<std::vector<std::uint8_t> >(
        api::Status(api::StatusCode::kIoError, "failed to determine file size"));
  }

  std::vector<std::uint8_t> data(static_cast<std::size_t>(sz));
  if (sz > 0) {
    std::size_t n = std::fread(data.data(), 1, data.size(), fp);
    if (n != data.size()) {
      std::fclose(fp);
      return api::Result<std::vector<std::uint8_t> >(
          api::Status(api::StatusCode::kIoError, "read incomplete"));
    }
  }

  std::fclose(fp);
  return api::Result<std::vector<std::uint8_t> >(data);
}

api::Status FileUtils::WriteAll(const char* path, const void* data, std::size_t size) {
  if (path == NULL || path[0] == '\0') {
    return api::Status(api::StatusCode::kInvalidArgument, "path is empty");
  }

  std::FILE* fp = std::fopen(path, "wb");
  if (fp == NULL) {
    return api::Status(api::StatusCode::kIoError, "failed to create file");
  }

  if (size > 0 && data != NULL) {
    std::size_t n = std::fwrite(data, 1, size, fp);
    if (n != size) {
      std::fclose(fp);
      return api::Status(api::StatusCode::kIoError, "write incomplete");
    }
  }

  std::fclose(fp);
  return api::Status::Ok();
}

}  // namespace io
}  // namespace corekit
