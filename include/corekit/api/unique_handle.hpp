#pragma once

#include <cstddef>

namespace corekit {

/// RAII wrapper for any corekit component that exposes Release().
/// Prevents resource leaks by automatically calling Release() on destruction.
///
/// Usage:
///   auto exec = corekit::UniqueHandle<task::IExecutor>(corekit_create_executor());
///   exec->Submit(fn, data);
///   // automatically released when exec goes out of scope
template <typename T>
class UniqueHandle {
 public:
  UniqueHandle() : ptr_(NULL) {}
  explicit UniqueHandle(T* p) : ptr_(p) {}

  ~UniqueHandle() { Reset(); }

  // Move only — no copies.
  UniqueHandle(UniqueHandle&& other) : ptr_(other.ptr_) { other.ptr_ = NULL; }

  UniqueHandle& operator=(UniqueHandle&& other) {
    if (this != &other) {
      Reset();
      ptr_ = other.ptr_;
      other.ptr_ = NULL;
    }
    return *this;
  }

  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* Get() const { return ptr_; }

  explicit operator bool() const { return ptr_ != NULL; }

  /// Release ownership without destroying. Caller takes responsibility.
  T* Detach() {
    T* p = ptr_;
    ptr_ = NULL;
    return p;
  }

  /// Destroy current and optionally take a new pointer.
  void Reset(T* p = NULL) {
    if (ptr_) {
      ptr_->Release();
    }
    ptr_ = p;
  }

 private:
  UniqueHandle(const UniqueHandle&);
  UniqueHandle& operator=(const UniqueHandle&);

  T* ptr_;
};

// Convenience type aliases — available after including the relevant interface headers.
namespace log { class ILogManager; }
namespace task { class IExecutor; class ITaskGraph; }
namespace ipc { class IChannel; }
namespace memory { class IAllocator; class IMemoryPool; }
namespace io { class IFile; }

typedef UniqueHandle<log::ILogManager> LogManagerHandle;
typedef UniqueHandle<task::IExecutor> ExecutorHandle;
typedef UniqueHandle<task::ITaskGraph> TaskGraphHandle;
typedef UniqueHandle<ipc::IChannel> ChannelHandle;
typedef UniqueHandle<memory::IAllocator> AllocatorHandle;
typedef UniqueHandle<memory::IMemoryPool> MemoryPoolHandle;
typedef UniqueHandle<io::IFile> FileHandle;

}  // namespace corekit
