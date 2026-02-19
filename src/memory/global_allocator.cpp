#include "corekit/memory/i_global_allocator.hpp"

#include <memory>
#include <mutex>
#include <string>

#include "corekit/json/i_json.hpp"
#include "memory/system_allocator.hpp"
#if defined(COREKIT_ENABLE_MIMALLOC_BACKEND)
#include "memory/mimalloc_allocator.hpp"
#endif
#if defined(COREKIT_ENABLE_TBBMALLOC_BACKEND)
#include "memory/tbb_allocator.hpp"
#endif

namespace corekit {
namespace memory {

#define CK_STATUS(code, message) api::Status::FromModule((code), (message), api::ErrorModule::kMemory)
namespace {

std::mutex& ConfigureMu() {
  static std::mutex mu;
  return mu;
}

std::shared_ptr<IAllocator>& AllocatorSharedState() {
  static std::shared_ptr<IAllocator> state(new SystemAllocator());
  return state;
}

GlobalAllocatorOptions& GlobalOptions() {
  static GlobalAllocatorOptions opts;
  return opts;
}

std::shared_ptr<IAllocator> SnapshotAllocator() {
  return std::atomic_load(&AllocatorSharedState());
}

std::shared_ptr<IAllocator> EnsureAllocatorLocked() {
  std::shared_ptr<IAllocator> allocator = SnapshotAllocator();
  if (!allocator) {
    allocator.reset(new SystemAllocator());
    std::atomic_store(&AllocatorSharedState(), allocator);
    GlobalOptions().backend = AllocBackend::kSystem;
  }
  return allocator;
}

api::Result<std::shared_ptr<IAllocator> > CreateAllocator(AllocBackend backend) {
  switch (backend) {
    case AllocBackend::kSystem:
      return api::Result<std::shared_ptr<IAllocator> >(std::shared_ptr<IAllocator>(new SystemAllocator()));
#if defined(COREKIT_ENABLE_MIMALLOC_BACKEND)
    case AllocBackend::kMimalloc:
      return api::Result<std::shared_ptr<IAllocator> >(std::shared_ptr<IAllocator>(new MimallocAllocator()));
#endif
#if defined(COREKIT_ENABLE_TBBMALLOC_BACKEND)
    case AllocBackend::kTbbScalable:
      return api::Result<std::shared_ptr<IAllocator> >(std::shared_ptr<IAllocator>(new TbbAllocator()));
#endif
    default:
      return api::Result<std::shared_ptr<IAllocator> >(CK_STATUS(
          api::StatusCode::kUnsupported, "Requested backend is not enabled in this build"));
  }
}

api::Status ParseBackend(const std::string& value, AllocBackend* out) {
  if (value == "system") {
    *out = AllocBackend::kSystem;
    return api::Status::Ok();
  }
  if (value == "tbb" || value == "tbb_scalable" || value == "tbbscalable") {
    *out = AllocBackend::kTbbScalable;
    return api::Status::Ok();
  }
  if (value == "mimalloc" || value == "mi") {
    *out = AllocBackend::kMimalloc;
    return api::Status::Ok();
  }
  return CK_STATUS(api::StatusCode::kInvalidArgument, "memory.backend is invalid");
}

const char* BackendDisplayNameImpl(AllocBackend backend) {
  switch (backend) {
    case AllocBackend::kSystem:
      return "system";
    case AllocBackend::kMimalloc:
      return "mimalloc";
    case AllocBackend::kTbbScalable:
      return "tbb";
    default:
      return "unknown";
  }
}

bool IsBackendEnabledImpl(AllocBackend backend) {
  switch (backend) {
    case AllocBackend::kSystem:
      return true;
#if defined(COREKIT_ENABLE_MIMALLOC_BACKEND)
    case AllocBackend::kMimalloc:
      return true;
#endif
#if defined(COREKIT_ENABLE_TBBMALLOC_BACKEND)
    case AllocBackend::kTbbScalable:
      return true;
#endif
    default:
      return false;
  }
}
}  // namespace

api::Status GlobalAllocator::Configure(const GlobalAllocatorOptions& options) {
  std::lock_guard<std::mutex> lock(ConfigureMu());

  std::shared_ptr<IAllocator> current = EnsureAllocatorLocked();
  GlobalAllocatorOptions normalized = options;

  if (normalized.backend != GlobalOptions().backend) {
    AllocatorStats current_stats = current->Stats();
    if (current_stats.bytes_in_use != 0) {
      return CK_STATUS(api::StatusCode::kWouldBlock,
                       "cannot switch allocator backend while memory is still in use");
    }

    api::Result<std::shared_ptr<IAllocator> > created = CreateAllocator(normalized.backend);
    if (!created.ok()) {
      if (normalized.strict_backend) {
        return created.status();
      }
      api::Result<std::shared_ptr<IAllocator> > fallback = CreateAllocator(AllocBackend::kSystem);
      if (!fallback.ok()) {
        return fallback.status();
      }
      std::atomic_store(&AllocatorSharedState(), fallback.value());
      normalized.backend = AllocBackend::kSystem;
      GlobalOptions() = normalized;
      return api::Status::Ok();
    }

    std::atomic_store(&AllocatorSharedState(), created.value());
  }

  GlobalOptions() = normalized;
  return api::Status::Ok();
}

api::Status GlobalAllocator::ConfigureFromFile(const std::string& config_path) {
  api::Result<json::Json> loaded = json::JsonCodec::LoadFile(config_path);
  if (!loaded.ok()) {
    return loaded.status();
  }

  if (!loaded.value().is_object()) {
    return CK_STATUS(api::StatusCode::kInvalidArgument, "root JSON must be object");
  }

  GlobalAllocatorOptions options;
  {
    std::lock_guard<std::mutex> lock(ConfigureMu());
    options = GlobalOptions();
  }
  const json::Json& root = loaded.value();
  const json::Json* memory = NULL;

  if (root.contains("memory")) {
    memory = &root["memory"];
    if (!memory->is_object()) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "memory must be JSON object");
    }
  } else {
    memory = &root;
  }

  if (memory->contains("backend")) {
    if (!(*memory)["backend"].is_string()) {
      return CK_STATUS(api::StatusCode::kInvalidArgument, "memory.backend must be string");
    }
    AllocBackend backend = AllocBackend::kSystem;
    api::Status st = ParseBackend((*memory)["backend"].get<std::string>(), &backend);
    if (!st.ok()) {
      return st;
    }
    options.backend = backend;
  }

  if (memory->contains("strict_backend")) {
    if (!(*memory)["strict_backend"].is_boolean()) {
      return CK_STATUS(api::StatusCode::kInvalidArgument,
                       "memory.strict_backend must be boolean");
    }
    options.strict_backend = (*memory)["strict_backend"].get<bool>();
  }

  return Configure(options);
}

api::Result<void*> GlobalAllocator::Allocate(std::size_t size, std::size_t alignment) {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  std::shared_ptr<IAllocator> allocator = EnsureAllocatorLocked();
  return allocator->Allocate(size, alignment);
}

api::Status GlobalAllocator::Deallocate(void* ptr) {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  std::shared_ptr<IAllocator> allocator = EnsureAllocatorLocked();
  return allocator->Deallocate(ptr);
}

AllocBackend GlobalAllocator::CurrentBackend() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  return GlobalOptions().backend;
}

const char* GlobalAllocator::BackendDisplayName(AllocBackend backend) {
  return BackendDisplayNameImpl(backend);
}

bool GlobalAllocator::IsBackendEnabled(AllocBackend backend) {
  return IsBackendEnabledImpl(backend);
}

const char* GlobalAllocator::CurrentBackendName() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  return EnsureAllocatorLocked()->BackendName();
}

AllocatorCaps GlobalAllocator::CurrentCaps() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  return EnsureAllocatorLocked()->Caps();
}

AllocatorStats GlobalAllocator::CurrentStats() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  return EnsureAllocatorLocked()->Stats();
}

void GlobalAllocator::ResetCurrentStats() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  EnsureAllocatorLocked()->ResetStats();
}

#undef CK_STATUS

}  // namespace memory
}  // namespace corekit

