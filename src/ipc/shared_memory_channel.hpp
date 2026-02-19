#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "corekit/ipc/i_channel.hpp"

namespace corekit {
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
  struct PendingMessage {
    std::vector<std::uint8_t> bytes;
  };

  struct FrameHeader {
    std::uint32_t size;
    std::uint32_t reserved;
  };

  struct alignas(64) SharedHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t capacity;
    std::uint32_t message_max_bytes;
    std::uint32_t ring_bytes;
    std::uint32_t ring_mask;
    std::uint64_t reserved0;
    std::uint64_t reserved1;

    alignas(64) std::atomic<std::uint64_t> write_index;
    alignas(64) std::atomic<std::uint64_t> read_index;

    alignas(64) std::atomic<std::uint64_t> send_ok;
    std::atomic<std::uint64_t> recv_ok;
    std::atomic<std::uint64_t> dropped_when_full;
  };

  api::Status ValidateOptions(const ChannelOptions& options) const;
  std::size_t TotalBytes() const;
  std::size_t LocalOutboxLimit() const;
  std::size_t FrameBytes(std::uint32_t payload_size) const;
  std::size_t RingBytes() const;
  std::size_t RingMask() const;
  std::uint8_t* RingBase() const;
  std::size_t ContiguousFrom(std::uint64_t index) const;
  std::size_t UsedBytes(std::uint64_t write, std::uint64_t read) const;
  api::Status TryWriteOneToShared(const void* data, std::uint32_t size);
  void ProcessIoOnce(std::size_t write_budget);
  api::Status MapAsServer(const ChannelOptions& options);
  api::Status MapAsClient(const ChannelOptions& options);

  std::string shared_name_;
  ChannelOptions options_;
  std::atomic<std::uint64_t> local_would_block_send_;
  std::atomic<std::uint64_t> local_would_block_recv_;
  std::atomic<std::uint64_t> local_pending_drop_;
  std::deque<PendingMessage> local_outbox_;
  bool opened_;

#if defined(_WIN32)
  void* mapping_handle_;
  void* mapping_view_;
#endif

  SharedHeader* header_;
};

}  // namespace ipc
}  // namespace corekit
