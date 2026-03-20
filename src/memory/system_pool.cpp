#include "corekit/memory/system_pool.hpp"

#include <mutex>

#include "corekit/json/i_json.hpp"
#include "memory/slab_pool_impl.hpp"

namespace corekit {
namespace memory {

namespace {

std::mutex& PoolMutex() {
  static std::mutex mu;
  return mu;
}

PoolConfig& PendingConfig() {
  static PoolConfig cfg;
  return cfg;
}

bool& ConfigApplied() {
  static bool applied = false;
  return applied;
}

SlabPoolImpl*& PoolInstance() {
  static SlabPoolImpl* inst = NULL;
  return inst;
}

SlabPoolImpl* EnsurePoolLocked() {
  if (PoolInstance() == NULL) {
    PoolInstance() = new SlabPoolImpl();
    PoolConfig& cfg = PendingConfig();
    if (!ConfigApplied()) {
      cfg.name = "system";
    }
    PoolInstance()->Init(cfg);
    ConfigApplied() = true;
  }
  return PoolInstance();
}

}  // namespace

api::Status SystemPool::Configure(const PoolConfig& config) {
  std::lock_guard<std::mutex> lock(PoolMutex());
  if (PoolInstance() != NULL) {
    return api::Status(api::StatusCode::kAlreadyInitialized,
                       "system pool already initialized; configure before first use");
  }
  PendingConfig() = config;
  ConfigApplied() = true;
  return api::Status::Ok();
}

api::Status SystemPool::ConfigureFromFile(const std::string& config_path) {
  api::Result<json::Json> loaded = json::JsonCodec::LoadFile(config_path);
  if (!loaded.ok()) {
    return loaded.status();
  }

  const json::Json& root = loaded.value();
  if (!root.is_object()) {
    return api::Status(api::StatusCode::kInvalidArgument, "root JSON must be object");
  }

  PoolConfig cfg;
  cfg.name = "system";

  const json::Json* pool_section = NULL;
  if (root.contains("pool")) {
    pool_section = &root["pool"];
  } else if (root.contains("memory") && root["memory"].is_object() &&
             root["memory"].contains("pool")) {
    pool_section = &root["memory"]["pool"];
  }

  if (pool_section != NULL && pool_section->is_object()) {
    const json::Json& ps = *pool_section;

    if (ps.contains("name") && ps["name"].is_string()) {
      // Store name in a static buffer for the const char* pointer.
      static std::string pool_name;
      pool_name = ps["name"].get<std::string>();
      cfg.name = pool_name.c_str();
    }
    if (ps.contains("block_size") && ps["block_size"].is_number_unsigned()) {
      cfg.block_size = ps["block_size"].get<std::size_t>();
    }
    if (ps.contains("max_bytes") && ps["max_bytes"].is_number_unsigned()) {
      cfg.max_bytes = ps["max_bytes"].get<std::size_t>();
    }
    if (ps.contains("max_cached_blocks") && ps["max_cached_blocks"].is_number_unsigned()) {
      cfg.max_cached_blocks = ps["max_cached_blocks"].get<std::size_t>();
    }
    if (ps.contains("enable_guard_bytes") && ps["enable_guard_bytes"].is_boolean()) {
      cfg.enable_guard_bytes = ps["enable_guard_bytes"].get<bool>();
    }
    if (ps.contains("thread_safe") && ps["thread_safe"].is_boolean()) {
      cfg.thread_safe = ps["thread_safe"].get<bool>();
    }
  }

  return Configure(cfg);
}

IMemoryPool* SystemPool::Instance() {
  std::lock_guard<std::mutex> lock(PoolMutex());
  return EnsurePoolLocked();
}

PoolStats SystemPool::CurrentStats() {
  std::lock_guard<std::mutex> lock(PoolMutex());
  SlabPoolImpl* inst = EnsurePoolLocked();
  return inst->Stats();
}

api::Status SystemPool::Shrink() {
  std::lock_guard<std::mutex> lock(PoolMutex());
  SlabPoolImpl* inst = EnsurePoolLocked();
  return inst->Shrink();
}

}  // namespace memory
}  // namespace corekit
