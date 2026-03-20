#include "ipc/shm_backend.hpp"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace corekit {
namespace ipc {

class Win32ShmBackend : public IShmBackend {
 public:
  Win32ShmBackend() : handle_(NULL), view_(NULL), size_(0) {}

  ~Win32ShmBackend() override { Close(); }

  api::Status Create(const std::string& name, std::size_t size) override {
    if (view_ != NULL) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "already mapped");
    }
    if (size == 0 || size > static_cast<std::size_t>(0xFFFFFFFFu)) {
      return api::Status(api::StatusCode::kInvalidArgument, "invalid size");
    }

    const DWORD bytes = static_cast<DWORD>(size);
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                        0, bytes, name.c_str());
    if (mapping == NULL) {
      return api::Status(api::StatusCode::kIoError, "CreateFileMapping failed");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      CloseHandle(mapping);
      return api::Status(api::StatusCode::kAlreadyInitialized, "shared memory already exists");
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (view == NULL) {
      CloseHandle(mapping);
      return api::Status(api::StatusCode::kIoError, "MapViewOfFile failed");
    }

    handle_ = mapping;
    view_ = view;
    size_ = size;
    return api::Status::Ok();
  }

  api::Status Open(const std::string& name, std::size_t min_size) override {
    if (view_ != NULL) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "already mapped");
    }

    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (mapping == NULL) {
      return api::Status(api::StatusCode::kNotFound, "OpenFileMapping failed, server not ready");
    }

    // Map at least min_size if known, otherwise map header first.
    const std::size_t map_size = min_size > 0 ? min_size : 0;
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0,
                               static_cast<SIZE_T>(map_size));
    if (view == NULL) {
      CloseHandle(mapping);
      return api::Status(api::StatusCode::kIoError, "MapViewOfFile failed");
    }

    handle_ = mapping;
    view_ = view;
    size_ = map_size;
    return api::Status::Ok();
  }

  void* BaseAddress() const override { return view_; }
  std::size_t MappedSize() const override { return size_; }
  bool IsOpen() const override { return view_ != NULL; }

  void Close() override {
    if (view_ != NULL) {
      UnmapViewOfFile(view_);
      view_ = NULL;
    }
    if (handle_ != NULL) {
      CloseHandle(handle_);
      handle_ = NULL;
    }
    size_ = 0;
  }

 private:
  HANDLE handle_;
  void* view_;
  std::size_t size_;
};

IShmBackend* CreateShmBackend() { return new Win32ShmBackend(); }

}  // namespace ipc
}  // namespace corekit

#endif  // _WIN32
