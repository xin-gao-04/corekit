#include "ipc/shared_memory_channel.hpp"

#include <cstring>
#include <sstream>

#include "liblogkit/api/version.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace liblogkit {
namespace ipc {
namespace {

static const std::uint32_t kChannelMagic = 0x4C4B4950;  // "LKIP"
static const std::uint32_t kChannelVersion = 1;

std::string BuildSharedName(const std::string& name) {
  return std::string("Local\\liblogkit.") + name;
}

std::string ToString(std::uint32_t value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

}  // namespace

SharedMemoryChannel::SharedMemoryChannel()
    : local_would_block_send_(0),
      local_would_block_recv_(0),
      opened_(false),
#if defined(_WIN32)
      mapping_handle_(NULL),
      mapping_view_(NULL),
#endif
      header_(NULL) {}

SharedMemoryChannel::~SharedMemoryChannel() { Close(); }

const char* SharedMemoryChannel::Name() const { return "liblogkit.ipc.shm_ring_v1"; }

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

std::size_t SharedMemoryChannel::SlotStride() const {
  const std::size_t base = sizeof(SlotHeader) + options_.message_max_bytes;
  const std::size_t align = sizeof(std::uint64_t);
  return ((base + align - 1) / align) * align;
}

std::size_t SharedMemoryChannel::TotalBytes() const {
  return sizeof(SharedHeader) + static_cast<std::size_t>(options_.capacity) * SlotStride();
}

SharedMemoryChannel::SlotHeader* SharedMemoryChannel::GetSlot(std::uint64_t index) const {
  std::uint8_t* base = reinterpret_cast<std::uint8_t*>(header_) + sizeof(SharedHeader);
  return reinterpret_cast<SlotHeader*>(
      base + (index % options_.capacity) * static_cast<std::uint64_t>(SlotStride()));
}

std::uint8_t* SharedMemoryChannel::GetPayload(SlotHeader* slot) const {
  return reinterpret_cast<std::uint8_t*>(slot) + sizeof(SlotHeader);
}

api::Status SharedMemoryChannel::OpenServer(const ChannelOptions& options) {
  if (opened_) {
    return api::Status(api::StatusCode::kAlreadyInitialized, "channel already opened");
  }
  api::Status st = ValidateOptions(options);
  if (!st.ok()) {
    return st;
  }
#if defined(_WIN32)
  options_ = options;
  shared_name_ = BuildSharedName(options_.name);
  return MapAsServer(options_);
#else
  (void)options;
  return api::Status(api::StatusCode::kUnsupported,
                     "OpenServer is currently implemented for Windows only");
#endif
}

api::Status SharedMemoryChannel::OpenClient(const ChannelOptions& options) {
  if (opened_) {
    return api::Status(api::StatusCode::kAlreadyInitialized, "channel already opened");
  }
  if (options.name.empty()) {
    return api::Status(api::StatusCode::kInvalidArgument, "channel name is empty");
  }
#if defined(_WIN32)
  options_ = options;
  shared_name_ = BuildSharedName(options.name);
  return MapAsClient(options);
#else
  (void)options;
  return api::Status(api::StatusCode::kUnsupported,
                     "OpenClient is currently implemented for Windows only");
#endif
}

api::Status SharedMemoryChannel::Close() {
#if defined(_WIN32)
  if (mapping_view_ != NULL) {
    UnmapViewOfFile(mapping_view_);
    mapping_view_ = NULL;
  }
  if (mapping_handle_ != NULL) {
    CloseHandle(reinterpret_cast<HANDLE>(mapping_handle_));
    mapping_handle_ = NULL;
  }
#endif
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

  const std::uint64_t write = header_->write_index.load(std::memory_order_acquire);
  const std::uint64_t read = header_->read_index.load(std::memory_order_acquire);
  if (write - read >= options_.capacity) {
    local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
    if (options_.drop_when_full) {
      header_->dropped_when_full.fetch_add(1, std::memory_order_relaxed);
    }
    return api::Status(api::StatusCode::kWouldBlock, "channel queue is full");
  }

  SlotHeader* slot = GetSlot(write);
  if (slot->state.load(std::memory_order_acquire) != 0) {
    local_would_block_send_.fetch_add(1, std::memory_order_relaxed);
    return api::Status(api::StatusCode::kWouldBlock, "slot is not ready for writing");
  }

  if (size > 0) {
    std::memcpy(GetPayload(slot), data, size);
  }
  slot->size = size;
  slot->state.store(1, std::memory_order_release);
  header_->write_index.store(write + 1, std::memory_order_release);
  header_->send_ok.fetch_add(1, std::memory_order_relaxed);
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

  const std::uint64_t read = header_->read_index.load(std::memory_order_acquire);
  const std::uint64_t write = header_->write_index.load(std::memory_order_acquire);
  if (read >= write) {
    local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kWouldBlock, "channel has no message"));
  }

  SlotHeader* slot = GetSlot(read);
  if (slot->state.load(std::memory_order_acquire) != 1) {
    local_would_block_recv_.fetch_add(1, std::memory_order_relaxed);
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kWouldBlock, "slot is not readable yet"));
  }

  const std::uint32_t required = slot->size;
  if (required > buffer_size) {
    return api::Result<std::uint32_t>(
        api::Status(api::StatusCode::kBufferTooSmall,
                    "buffer too small, required=" + ToString(required)));
  }

  if (required > 0) {
    std::memcpy(buffer, GetPayload(slot), required);
  }
  slot->state.store(0, std::memory_order_release);
  header_->read_index.store(read + 1, std::memory_order_release);
  header_->recv_ok.fetch_add(1, std::memory_order_relaxed);
  return api::Result<std::uint32_t>(required);
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
#if defined(_WIN32)
  const DWORD bytes = static_cast<DWORD>(TotalBytes());
  HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, bytes,
                                      shared_name_.c_str());
  if (mapping == NULL) {
    return api::Status(api::StatusCode::kIoError, "CreateFileMapping failed");
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mapping);
    return api::Status(api::StatusCode::kAlreadyInitialized,
                       "channel already exists, server should be unique");
  }

  void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  if (view == NULL) {
    CloseHandle(mapping);
    return api::Status(api::StatusCode::kIoError, "MapViewOfFile failed");
  }

  mapping_handle_ = mapping;
  mapping_view_ = view;
  header_ = reinterpret_cast<SharedHeader*>(mapping_view_);
  std::memset(mapping_view_, 0, bytes);

  header_->magic = kChannelMagic;
  header_->version = kChannelVersion;
  header_->capacity = options.capacity;
  header_->message_max_bytes = options.message_max_bytes;
  header_->write_index.store(0, std::memory_order_relaxed);
  header_->read_index.store(0, std::memory_order_relaxed);
  header_->send_ok.store(0, std::memory_order_relaxed);
  header_->recv_ok.store(0, std::memory_order_relaxed);
  header_->dropped_when_full.store(0, std::memory_order_relaxed);

  for (std::uint32_t i = 0; i < options.capacity; ++i) {
    SlotHeader* slot = GetSlot(i);
    slot->state.store(0, std::memory_order_relaxed);
    slot->size = 0;
  }

  opened_ = true;
  return api::Status::Ok();
#else
  (void)options;
  return api::Status(api::StatusCode::kUnsupported,
                     "MapAsServer is currently implemented for Windows only");
#endif
}

api::Status SharedMemoryChannel::MapAsClient(const ChannelOptions&) {
#if defined(_WIN32)
  HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shared_name_.c_str());
  if (mapping == NULL) {
    return api::Status(api::StatusCode::kNotFound, "OpenFileMapping failed, server not ready");
  }

  void* header_view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader));
  if (header_view == NULL) {
    CloseHandle(mapping);
    return api::Status(api::StatusCode::kIoError, "MapViewOfFile header failed");
  }

  SharedHeader* hdr = reinterpret_cast<SharedHeader*>(header_view);
  if (hdr->magic != kChannelMagic || hdr->version != kChannelVersion) {
    UnmapViewOfFile(header_view);
    CloseHandle(mapping);
    return api::Status(api::StatusCode::kInternalError, "channel header magic/version mismatch");
  }

  options_.capacity = hdr->capacity;
  options_.message_max_bytes = hdr->message_max_bytes;
  const std::size_t total = TotalBytes();

  UnmapViewOfFile(header_view);
  void* full_view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, total);
  if (full_view == NULL) {
    CloseHandle(mapping);
    return api::Status(api::StatusCode::kIoError, "MapViewOfFile full failed");
  }

  mapping_handle_ = mapping;
  mapping_view_ = full_view;
  header_ = reinterpret_cast<SharedHeader*>(mapping_view_);
  opened_ = true;
  return api::Status::Ok();
#else
  return api::Status(api::StatusCode::kUnsupported,
                     "MapAsClient is currently implemented for Windows only");
#endif
}

}  // namespace ipc
}  // namespace liblogkit
