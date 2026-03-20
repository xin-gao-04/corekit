#include "ipc/shared_memory_channel.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

#include "corekit/api/version.hpp"

namespace corekit {
namespace ipc {
namespace {

static const std::uint32_t kChannelMagic = 0x4C4B4950;  // "LKIP"
static const std::uint32_t kChannelVersion = 2;
static const std::uint32_t kFrameData = 0;
static const std::uint32_t kFrameWrap = 1;

std::string BuildSharedName(const std::string& name) {
#if defined(_WIN32)
  return std::string("Local\\corekit.") + name;
#else
  return std::string("/corekit.") + name;
#endif
}

std::string ToString(std::uint32_t value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

std::size_t AlignUp(std::size_t value, std::size_t align) {
  return ((value + align - 1) / align) * align;
}

std::uint32_t NextPow2(std::uint32_t v) {
  if (v <= 1u) {
    return 1u;
  }
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

}  // namespace

SharedMemoryChannel::SharedMemoryChannel()
    : local_would_block_send_(0),
      local_would_block_recv_(0),
      local_pending_drop_(0),
      opened_(false),
      backend_(NULL),
      header_(NULL) {}

SharedMemoryChannel::~SharedMemoryChannel() {
  Close();
  delete backend_;
}

const char* SharedMemoryChannel::Name() const { return "corekit.ipc.shm_ring_v2"; }

std::uint32_t SharedMemoryChannel::ApiVersion() const { return api::kApiVersion; }

void SharedMemoryChannel::Release() { delete this; }

api::Status SharedMemoryChannel::ValidateOptions(const ChannelOptions& options) const {
  if (options.name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "channel name is empty");
  }
  if (options.capacity == 0) {
    return api::Status(api::StatusCode::kInvalidArgument, "capacity must be > 0");
  }
  if (options.message_max_bytes == 0) {
    return api::Status(api::StatusCode::kInvalidArgument,
                       "message_max_bytes must be > 0");
  }
  return api::Status::Ok();
}

std::size_t SharedMemoryChannel::FrameBytes(std::uint32_t payload_size) const {
  return AlignUp(sizeof(FrameHeader) + static_cast<std::size_t>(payload_size), sizeof(std::uint64_t));
}

std::size_t SharedMemoryChannel::RingBytes() const {
  return header_ == NULL ? 0u : static_cast<std::size_t>(header_->ring_bytes);
}

std::size_t SharedMemoryChannel::RingMask() const {
  return header_ == NULL ? 0u : static_cast<std::size_t>(header_->ring_mask);
}

std::size_t SharedMemoryChannel::TotalBytes() const {
  const std::size_t stride = FrameBytes(options_.message_max_bytes);
  const std::size_t target = stride * static_cast<std::size_t>(options_.capacity);
  if (target > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return 0;
  }
  const std::uint32_t ring = NextPow2(static_cast<std::uint32_t>(target));
  return sizeof(SharedHeader) + static_cast<std::size_t>(ring);
}

std::size_t SharedMemoryChannel::LocalOutboxLimit() const {
  const std::size_t cap = static_cast<std::size_t>(options_.capacity);
  return std::max<std::size_t>(4, cap * 2);
}

std::uint8_t* SharedMemoryChannel::RingBase() const {
  return reinterpret_cast<std::uint8_t*>(header_) + sizeof(SharedHeader);
}

std::size_t SharedMemoryChannel::ContiguousFrom(std::uint64_t index) const {
  const std::size_t off = static_cast<std::size_t>(index) & RingMask();
  return RingBytes() - off;
}

std::size_t SharedMemoryChannel::UsedBytes(std::uint64_t write, std::uint64_t read) const {
  if (write < read) {
    return RingBytes();
  }
  const std::uint64_t used = write - read;
  if (used > static_cast<std::uint64_t>(RingBytes())) {
    return RingBytes();
  }
  return static_cast<std::size_t>(used);
}

api::Status SharedMemoryChannel::TryWriteOneToShared(const void* data, std::uint32_t size) {
  const std::size_t frame_bytes = FrameBytes(size);
  if (frame_bytes > RingBytes()) {
    return api::Status(api::StatusCode::kInvalidArgument, "frame exceeds ring size");
  }

  std::uint64_t write = header_->write_index.load(std::memory_order_acquire);
  std::uint64_t read = header_->read_index.load(std::memory_order_acquire);

  std::size_t used = UsedBytes(write, read);
  std::size_t free_bytes = RingBytes() - used;

  std::size_t contiguous = ContiguousFrom(write);
  if (contiguous < sizeof(FrameHeader) || contiguous < frame_bytes) {
    if (free_bytes < contiguous + frame_bytes) {
      local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
      return api::Status(api::StatusCode::kWouldBlock, "channel queue is full");
    }
    if (contiguous >= sizeof(FrameHeader)) {
      const std::size_t tail_off = static_cast<std::size_t>(write) & RingMask();
      FrameHeader* wrap = reinterpret_cast<FrameHeader*>(RingBase() + tail_off);
      wrap->size = 0;
      wrap->reserved = kFrameWrap;
    }
    write += static_cast<std::uint64_t>(contiguous);
    header_->write_index.store(write, std::memory_order_release);

    read = header_->read_index.load(std::memory_order_acquire);
    used = UsedBytes(write, read);
    free_bytes = RingBytes() - used;
    if (free_bytes < frame_bytes) {
      local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
      return api::Status(api::StatusCode::kWouldBlock, "channel queue is full");
    }
  } else if (free_bytes < frame_bytes) {
    local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
    return api::Status(api::StatusCode::kWouldBlock, "channel queue is full");
  }

  const std::size_t off = static_cast<std::size_t>(write) & RingMask();
  std::uint8_t* ptr = RingBase() + off;
  FrameHeader* frame = reinterpret_cast<FrameHeader*>(ptr);
  frame->size = size;
  frame->reserved = kFrameData;

  if (size > 0) {
    std::memcpy(ptr + sizeof(FrameHeader), data, size);
  }

  const std::size_t pad = frame_bytes - sizeof(FrameHeader) - static_cast<std::size_t>(size);
  if (pad > 0) {
    std::memset(ptr + sizeof(FrameHeader) + size, 0, pad);
  }

  header_->write_index.store(write + static_cast<std::uint64_t>(frame_bytes), std::memory_order_release);
  header_->send_ok.fetch_add(1, std::memory_order_relaxed);
  return api::Status::Ok();
}

void SharedMemoryChannel::ProcessIoOnce(std::size_t write_budget) {
  std::size_t remaining = write_budget;
  while (remaining > 0 && !local_outbox_.empty()) {
    const PendingMessage& msg = local_outbox_.front();
    const void* data = msg.bytes.empty() ? NULL : static_cast<const void*>(msg.bytes.data());
    const std::uint32_t size = static_cast<std::uint32_t>(msg.bytes.size());
    api::Status st = TryWriteOneToShared(data, size);
    if (!st.ok()) {
      if (st.code() == api::StatusCode::kWouldBlock) {
        break;
      }
      local_outbox_.pop_front();
      continue;
    }
    local_outbox_.pop_front();
    --remaining;
  }
}

api::Status SharedMemoryChannel::OpenServer(const ChannelOptions& options) {
  if (opened_) {
    return api::Status(api::StatusCode::kAlreadyInitialized, "channel already opened");
  }
  api::Status st = ValidateOptions(options);
  if (!st.ok()) {
    return st;
  }
  options_ = options;
  shared_name_ = BuildSharedName(options_.name);
  local_outbox_.clear();
  local_pending_drop_.store(0, std::memory_order_relaxed);
  return MapAsServer(options_);
}

api::Status SharedMemoryChannel::OpenClient(const ChannelOptions& options) {
  if (opened_) {
    return api::Status(api::StatusCode::kAlreadyInitialized, "channel already opened");
  }
  if (options.name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "channel name is empty");
  }
  options_ = options;
  shared_name_ = BuildSharedName(options.name);
  local_outbox_.clear();
  local_pending_drop_.store(0, std::memory_order_relaxed);
  return MapAsClient(options);
}

api::Status SharedMemoryChannel::Close() {
  if (backend_ != NULL) {
    backend_->Close();
  }
  local_outbox_.clear();
  header_ = NULL;
  opened_ = false;
  return api::Status::Ok();
}

api::Status SharedMemoryChannel::TrySend(const void* data, std::uint32_t size) {
  if (!opened_ || header_ == NULL) {
    return api::Status(api::StatusCode::kNotInitialized, "channel is not opened");
  }
  if (size > 0 && data == NULL) {
    return api::Status(api::StatusCode::kInvalidArgument, "data is null");
  }
  if (size > options_.message_max_bytes) {
    return api::Status(api::StatusCode::kInvalidArgument, "message exceeds max bytes");
  }

  ProcessIoOnce(1);

  if (local_outbox_.size() >= LocalOutboxLimit()) {
    local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
    if (options_.drop_when_full) {
      local_pending_drop_.fetch_add(1, std::memory_order_relaxed);
      header_->dropped_when_full.fetch_add(1, std::memory_order_relaxed);
    }
    return api::Status(api::StatusCode::kWouldBlock, "local pending queue is full");
  }

  PendingMessage msg;
  msg.bytes.resize(size);
  if (size > 0) {
    std::memcpy(msg.bytes.data(), data, size);
  }
  local_outbox_.push_back(std::move(msg));

  const std::size_t flush_budget = std::max<std::size_t>(1, std::min<std::size_t>(8, LocalOutboxLimit()));
  ProcessIoOnce(flush_budget);
  return api::Status::Ok();
}

api::Result<std::uint32_t> SharedMemoryChannel::TryRecv(void* buffer,
                                                        std::uint32_t buffer_size) {
  if (!opened_ || header_ == NULL) {
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kNotInitialized, "channel is not opened"));
  }
  if (buffer_size > 0 && buffer == NULL) {
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kInvalidArgument, "buffer is null"));
  }

  ProcessIoOnce(1);

  std::uint64_t read = header_->read_index.load(std::memory_order_acquire);
  std::uint64_t write = header_->write_index.load(std::memory_order_acquire);
  if (read >= write) {
    local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kWouldBlock, "channel has no message"));
  }

  std::size_t contiguous = ContiguousFrom(read);
  if (contiguous < sizeof(FrameHeader)) {
    read += static_cast<std::uint64_t>(contiguous);
    header_->read_index.store(read, std::memory_order_release);
    write = header_->write_index.load(std::memory_order_acquire);
    if (read >= write) {
      local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
      return api::Result<std::uint32_t>(
          api::Status(api::StatusCode::kWouldBlock, "channel has no message"));
    }
    contiguous = ContiguousFrom(read);
  }

  while (true) {
    const std::size_t off = static_cast<std::size_t>(read) & RingMask();
    const std::uint8_t* ptr = RingBase() + off;
    const FrameHeader* frame = reinterpret_cast<const FrameHeader*>(ptr);

    if (frame->reserved == kFrameWrap) {
      read += static_cast<std::uint64_t>(contiguous);
      header_->read_index.store(read, std::memory_order_release);
      write = header_->write_index.load(std::memory_order_acquire);
      if (read >= write) {
        local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
        return api::Result<std::uint32_t>(
            api::Status(api::StatusCode::kWouldBlock, "channel has no message"));
      }
      contiguous = ContiguousFrom(read);
      if (contiguous < sizeof(FrameHeader)) {
        read += static_cast<std::uint64_t>(contiguous);
        header_->read_index.store(read, std::memory_order_release);
        write = header_->write_index.load(std::memory_order_acquire);
        if (read >= write) {
          local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
          return api::Result<std::uint32_t>(
              api::Status(api::StatusCode::kWouldBlock, "channel has no message"));
        }
        contiguous = ContiguousFrom(read);
      }
      continue;
    }

    if (frame->reserved != kFrameData) {
      return api::Result<std::uint32_t>(
          api::Status(api::StatusCode::kInternalError, "corrupted frame marker"));
    }

    const std::uint32_t required = frame->size;
    if (required > options_.message_max_bytes) {
      return api::Result<std::uint32_t>(
          api::Status(api::StatusCode::kInternalError, "corrupted frame size"));
    }

    const std::size_t frame_bytes = FrameBytes(required);
    if (frame_bytes > contiguous || read + static_cast<std::uint64_t>(frame_bytes) > write) {
      local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
      return api::Result<std::uint32_t>(
          api::Status(api::StatusCode::kWouldBlock, "incomplete frame"));
    }

    if (required > buffer_size) {
      return api::Result<std::uint32_t>(
          api::Status(api::StatusCode::kBufferTooSmall,
                      "buffer too small, required=" + ToString(required)));
    }

    if (required > 0) {
      std::memcpy(buffer, ptr + sizeof(FrameHeader), required);
    }

    header_->read_index.store(read + static_cast<std::uint64_t>(frame_bytes), std::memory_order_release);
    header_->recv_ok.fetch_add(1, std::memory_order_relaxed);
    return api::Result<std::uint32_t>(required);
  }
}

ChannelStats SharedMemoryChannel::GetStats() const {
  ChannelStats out;
  if (header_ != NULL) {
    out.send_ok = header_->send_ok.load(std::memory_order_relaxed);
    out.recv_ok = header_->recv_ok.load(std::memory_order_relaxed);
    out.dropped_when_full = header_->dropped_when_full.load(std::memory_order_relaxed);
  }
  out.would_block_send = local_would_block_send_.load(std::memory_order_relaxed);
  out.would_block_recv = local_would_block_recv_.load(std::memory_order_relaxed);
  return out;
}

api::Status SharedMemoryChannel::MapAsServer(const ChannelOptions& options) {
  const std::size_t total_bytes = TotalBytes();
  if (total_bytes == 0) {
    return api::Status(api::StatusCode::kInvalidArgument, "channel memory size is too large");
  }

  if (backend_ == NULL) {
    backend_ = CreateShmBackend();
  }

  api::Status st = backend_->Create(shared_name_, total_bytes);
  if (!st.ok()) {
    return st;
  }

  void* base = backend_->BaseAddress();
  std::memset(base, 0, total_bytes);

  header_ = reinterpret_cast<SharedHeader*>(base);

  const std::uint32_t stride = static_cast<std::uint32_t>(FrameBytes(options.message_max_bytes));
  const std::uint32_t target = stride * options.capacity;
  const std::uint32_t ring_bytes = NextPow2(target);

  header_->magic = kChannelMagic;
  header_->version = kChannelVersion;
  header_->capacity = options.capacity;
  header_->message_max_bytes = options.message_max_bytes;
  header_->ring_bytes = ring_bytes;
  header_->ring_mask = ring_bytes - 1;
  header_->write_index.store(0, std::memory_order_relaxed);
  header_->read_index.store(0, std::memory_order_relaxed);
  header_->send_ok.store(0, std::memory_order_relaxed);
  header_->recv_ok.store(0, std::memory_order_relaxed);
  header_->dropped_when_full.store(0, std::memory_order_relaxed);

  opened_ = true;
  return api::Status::Ok();
}

api::Status SharedMemoryChannel::MapAsClient(const ChannelOptions&) {
  if (backend_ == NULL) {
    backend_ = CreateShmBackend();
  }

  // First map just enough to read the header.
  api::Status st = backend_->Open(shared_name_, sizeof(SharedHeader));
  if (!st.ok()) {
    return st;
  }

  SharedHeader* hdr = reinterpret_cast<SharedHeader*>(backend_->BaseAddress());
  if (hdr->magic != kChannelMagic || hdr->version != kChannelVersion) {
    backend_->Close();
    return api::Status(api::StatusCode::kInternalError, "channel header magic/version mismatch");
  }
  if (hdr->ring_bytes == 0 || ((hdr->ring_bytes & (hdr->ring_bytes - 1)) != 0)) {
    backend_->Close();
    return api::Status(api::StatusCode::kInternalError, "channel ring_bytes is invalid");
  }

  options_.capacity = hdr->capacity;
  options_.message_max_bytes = hdr->message_max_bytes;
  const std::size_t total = sizeof(SharedHeader) + static_cast<std::size_t>(hdr->ring_bytes);

  // Re-map with full size.
  backend_->Close();
  st = backend_->Open(shared_name_, total);
  if (!st.ok()) {
    return st;
  }

  header_ = reinterpret_cast<SharedHeader*>(backend_->BaseAddress());
  opened_ = true;
  return api::Status::Ok();
}

}  // namespace ipc
}  // namespace corekit
