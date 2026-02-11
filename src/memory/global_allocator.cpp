#include "corekit/memory/i_global_allocator.hpp"

#include <memory>
#include <mutex>
#include <string>

#include "corekit/json/i_json.hpp"
#include "memory/system_allocator.hpp"

namespace corekit {
namespace memory {
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
  return api::Status(api::StatusCode::kInvalidArgument, "memory.backend is invalid");
}

}  // namespace

api::Status GlobalAllocator::Configure(const GlobalAllocatorOptions& options) {
  std::lock_guard<std::mutex> lock(ConfigureMu());

  std::shared_ptr<IAllocator> allocator = SnapshotAllocator();
  if (!allocator) {
    allocator.reset(new SystemAllocator());
    std::atomic_store(&AllocatorSharedState(), allocator);
  }

  api::Status set_st = allocator->SetBackend(options.backend);
  if (set_st.ok()) {
    GlobalOptions() = options;
    return api::Status::Ok();
  }

  if (options.strict_backend) {
    return set_st;
  }

  api::Status fallback_st = allocator->SetBackend(AllocBackend::kSystem);
  if (!fallback_st.ok()) {
    return fallback_st;
  }

  GlobalAllocatorOptions normalized = options;
  normalized.backend = AllocBackend::kSystem;
  GlobalOptions() = normalized;
  return api::Status::Ok();
}

api::Status GlobalAllocator::ConfigureFromFile(const std::string& config_path) {
  api::Result<json::Json> loaded = json::JsonCodec::LoadFile(config_path);
  if (!loaded.ok()) {
    return loaded.status();
  }

  if (!loaded.value().is_object()) {
    return api::Status(api::StatusCode::kInvalidArgument, "root JSON must be object");
  }

  GlobalAllocatorOptions options = GlobalOptions();
  const json::Json& root = loaded.value();
  const json::Json* memory = NULL;

  if (root.contains("memory")) {
    memory = &root["memory"];
    if (!memory->is_object()) {
      return api::Status(api::StatusCode::kInvalidArgument, "memory must be JSON object");
    }
  } else {
    memory = &root;
  }

  if (memory->contains("backend")) {
    if (!(*memory)["backend"].is_string()) {
      return api::Status(api::StatusCode::kInvalidArgument, "memory.backend must be string");
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
      return api::Status(api::StatusCode::kInvalidArgument,
                         "memory.strict_backend must be boolean");
    }
    options.strict_backend = (*memory)["strict_backend"].get<bool>();
  }

  return Configure(options);
}

api::Result<void*> GlobalAllocator::Allocate(std::size_t size, std::size_t alignment) {
  std::shared_ptr<IAllocator> allocator = SnapshotAllocator();
  if (!allocator) {
    std::lock_guard<std::mutex> lock(ConfigureMu());
    allocator = SnapshotAllocator();
    if (!allocator) {
      allocator.reset(new SystemAllocator());
      std::atomic_store(&AllocatorSharedState(), allocator);
    }
  }
  return allocator->Allocate(size, alignment);
}

api::Status GlobalAllocator::Deallocate(void* ptr) {
  std::shared_ptr<IAllocator> allocator = SnapshotAllocator();
  if (!allocator) {
    std::lock_guard<std::mutex> lock(ConfigureMu());
    allocator = SnapshotAllocator();
    if (!allocator) {
      allocator.reset(new SystemAllocator());
      std::atomic_store(&AllocatorSharedState(), allocator);
    }
  }
  return allocator->Deallocate(ptr);
}

AllocBackend GlobalAllocator::CurrentBackend() {
  std::lock_guard<std::mutex> lock(ConfigureMu());
  return GlobalOptions().backend;
}

}  // namespace memory
}  // namespace corekit
