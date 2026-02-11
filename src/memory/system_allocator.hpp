#pragma once

#include "liblogkit/memory/iallocator.hpp"

namespace liblogkit {
namespace memory {

class SystemAllocator : public IAllocator {
 public:
  SystemAllocator();
  ~SystemAllocator() override;

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status SetBackend(AllocBackend backend) override;
  api::Result<void*> Allocate(std::size_t size, std::size_t alignment) override;
  api::Status Deallocate(void* ptr) override;

 private:
  AllocBackend backend_;
};

}  // namespace memory
}  // namespace liblogkit
