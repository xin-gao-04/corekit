#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "corekit/memory/iallocator.hpp"

namespace corekit {
namespace memory {

class TbbAllocator : public IAllocator {
 public:
  TbbAllocator();
  ~TbbAllocator() override;

  const char* Name() const override;
  const char* BackendName() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  AllocatorCaps Caps() const override;
  AllocatorStats Stats() const override;
  void ResetStats() override;

  api::Status SetBackend(AllocBackend backend) override;
  api::Result<void*> Allocate(std::size_t size, std::size_t alignment) override;
  api::Status Deallocate(void* ptr) override;

 private:
  void RecordAllocFailure();
  void RecordAllocSuccess(void* ptr, std::size_t size);
  void RecordDeallocate(void* ptr);

  std::atomic<std::uint64_t> alloc_count_;
  std::atomic<std::uint64_t> free_count_;
  std::atomic<std::uint64_t> alloc_fail_count_;
  std::atomic<std::uint64_t> bytes_in_use_;
  std::atomic<std::uint64_t> bytes_peak_;

  mutable std::mutex size_mu_;
  std::unordered_map<void*, std::size_t> alloc_sizes_;
};

}  // namespace memory
}  // namespace corekit
