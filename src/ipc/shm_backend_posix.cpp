#include "ipc/shm_backend.hpp"

#if !defined(_WIN32)

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace corekit {
namespace ipc {

class PosixShmBackend : public IShmBackend {
 public:
  PosixShmBackend() : fd_(-1), base_(NULL), size_(0), is_owner_(false) {}

  ~PosixShmBackend() override { Close(); }

  api::Status Create(const std::string& name, std::size_t size) override {
    if (base_ != NULL) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "already mapped");
    }
    if (size == 0) {
      return api::Status(api::StatusCode::kInvalidArgument, "invalid size");
    }

    // Ensure name starts with '/' for POSIX shm.
    std::string shm_name = NormalizeName(name);

    // O_EXCL ensures we fail if it already exists.
    int fd = shm_open(shm_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
      if (errno == EEXIST) {
        return api::Status(api::StatusCode::kAlreadyInitialized,
                           "shared memory already exists");
      }
      return api::Status(api::StatusCode::kIoError,
                         std::string("shm_open failed: ") + std::strerror(errno));
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
      close(fd);
      shm_unlink(shm_name.c_str());
      return api::Status(api::StatusCode::kIoError,
                         std::string("ftruncate failed: ") + std::strerror(errno));
    }

    void* mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
      close(fd);
      shm_unlink(shm_name.c_str());
      return api::Status(api::StatusCode::kIoError,
                         std::string("mmap failed: ") + std::strerror(errno));
    }

    fd_ = fd;
    base_ = mapped;
    size_ = size;
    shm_name_ = shm_name;
    is_owner_ = true;
    return api::Status::Ok();
  }

  api::Status Open(const std::string& name, std::size_t min_size) override {
    if (base_ != NULL) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "already mapped");
    }

    std::string shm_name = NormalizeName(name);

    int fd = shm_open(shm_name.c_str(), O_RDWR, 0);
    if (fd < 0) {
      return api::Status(api::StatusCode::kNotFound,
                         std::string("shm_open failed: ") + std::strerror(errno));
    }

    // Determine actual size from the file if min_size is 0.
    std::size_t map_size = min_size;
    if (map_size == 0) {
      struct stat st;
      if (fstat(fd, &st) != 0) {
        close(fd);
        return api::Status(api::StatusCode::kIoError, "fstat failed");
      }
      map_size = static_cast<std::size_t>(st.st_size);
    }

    void* mapped = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
      close(fd);
      return api::Status(api::StatusCode::kIoError,
                         std::string("mmap failed: ") + std::strerror(errno));
    }

    fd_ = fd;
    base_ = mapped;
    size_ = map_size;
    shm_name_ = shm_name;
    is_owner_ = false;
    return api::Status::Ok();
  }

  void* BaseAddress() const override { return base_; }
  std::size_t MappedSize() const override { return size_; }
  bool IsOpen() const override { return base_ != NULL; }

  void Close() override {
    if (base_ != NULL) {
      munmap(base_, size_);
      base_ = NULL;
    }
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
    if (is_owner_ && !shm_name_.empty()) {
      shm_unlink(shm_name_.c_str());
    }
    size_ = 0;
    is_owner_ = false;
    shm_name_.clear();
  }

 private:
  static std::string NormalizeName(const std::string& name) {
    // POSIX shm names must start with '/'.
    if (!name.empty() && name[0] == '/') return name;
    return "/" + name;
  }

  int fd_;
  void* base_;
  std::size_t size_;
  std::string shm_name_;
  bool is_owner_;
};

IShmBackend* CreateShmBackend() { return new PosixShmBackend(); }

}  // namespace ipc
}  // namespace corekit

#endif  // !_WIN32
