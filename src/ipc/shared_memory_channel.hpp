#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "liblogkit/ipc/i_channel.hpp"

namespace liblogkit {
namespace ipc {

class SharedMemoryChannel : public IChannel {
 public:
  SharedMemoryChannel();
  ~SharedMemoryChannel() override;

  const char* Name() const override;
  std::uint32_t ApiVersion() const override;
  void Release() override;

  api::Status OpenServer(const ChannelOptions& options) override;
  api::Status OpenClient(const ChannelOptions& options) override;
  api::Status Close() override;
  api::Status TrySend(const void* data, std::uint32_t size) override;
  api::Result<std::uint32_t> TryRecv(void* buffer, std::uint32_t buffer_size) override;
  ChannelStats GetStats() const override;

 private:
  struct SlotHeader {
    std::atomic<std::uint32_t> state;
    std::uint32_t size;
  };

  struct SharedHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t capacity;
    std::uint32_t message_max_bytes;
    std::atomic<std::uint64_t> write_index;
    std::atomic<std::uint64_t> read_index;
    std::atomic<std::uint64_t> send_ok;
    std::atomic<std::uint64_t> recv_ok;
    std::atomic<std::uint64_t> dropped_when_full;
  };

  api::Status ValidateOptions(const ChannelOptions& options) const;
  std::size_t SlotStride() const;
  std::size_t TotalBytes() const;
  SlotHeader* GetSlot(std::uint64_t index) const;
  std::uint8_t* GetPayload(SlotHeader* slot) const;
  api::Status MapAsServer(const ChannelOptions& options);
  api::Status MapAsClient(const ChannelOptions& options);

  std::string shared_name_;
  ChannelOptions options_;
  std::atomic<std::uint64_t> local_would_block_send_;
  std::atomic<std::uint64_t> local_would_block_recv_;
  bool opened_;

#if defined(_WIN32)
  void* mapping_handle_;
  void* mapping_view_;
#endif

  SharedHeader* header_;
};

}  // namespace ipc
}  // namespace liblogkit
