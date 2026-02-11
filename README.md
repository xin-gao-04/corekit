# corekit

A dedicated C++ utility toolkit for engineering and simulation workloads, delivered as a dynamic library plus interface headers.

## What is implemented
- Interface-first API (`pure virtual` classes) under `include/corekit`.
- DLL factory boundary (`extern "C"`) for ABI-stable object creation.
- Logging interface adapter over legacy glog backend.
- IPC v1 interface and Windows shared-memory ring-buffer implementation.
- Allocator (`SystemAllocator`), executor (`ThreadPoolExecutor`), and task graph (`SimpleTaskGraph`) concrete implementations.
- Global allocator entry with macro-friendly usage (`COREKIT_ALLOC`, `COREKIT_FREE`, `COREKIT_NEW`, `COREKIT_DELETE`).
- JSON codec wrapper (`corekit::json::JsonCodec`) and JSON memory policy config.
- Internal container/pool implementations under `src/` using global allocator routing.
- Design docs and diagrams under `docs/`.
- Third-party vendor entry folder: `3party/`.

## Build
```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Quick usage (interface style)
```cpp
#include "corekit/corekit.hpp"

corekit::log::ILogManager* logger = corekit_create_log_manager();
auto st = logger->Init("my_app", "config/logging.conf");
if (st.ok()) {
  logger->Log(corekit::log::LogSeverity::kInfo, "hello");
  logger->Shutdown();
}
corekit_destroy_log_manager(logger);
```

## Container implementation usage (project-internal)
Concrete container implementations are intentionally placed under `src/` and are used by this project/tests.
Public SDK exposure remains interface-first (`i_*.hpp`) for stable DLL boundaries.

## Global memory allocation macros
```cpp
#include "corekit/corekit.hpp"

corekit::memory::GlobalAllocator::ConfigureFromFile("config/corekit.json");

void* p = COREKIT_ALLOC(256);
COREKIT_FREE(p);

int* v = COREKIT_NEW(int, 7);
COREKIT_DELETE(v);
```

Memory JSON schema:
- `memory.backend = "system|tbb|mimalloc"`
- `memory.strict_backend = true|false`

## IPC usage (v1)
- Server creates a named channel via `OpenServer`.
- Client connects via `OpenClient`.
- Data path uses `TrySend` / `TryRecv` (non-blocking).

## Public headers
- `include/corekit/corekit.hpp`
- `include/corekit/log/ilog_manager.hpp`
- `include/corekit/ipc/i_channel.hpp`
- `include/corekit/api/factory.hpp`
- `include/corekit/concurrent/i_queue.hpp`
- `include/corekit/concurrent/i_map.hpp`
- `include/corekit/memory/i_object_pool.hpp`
- `include/corekit/memory/i_global_allocator.hpp`
- `include/corekit/json/i_json.hpp`

## Notes
- Current IPC backend implementation is Windows-first (`CreateFileMapping` based).
- API comments are attached directly to virtual methods, focused on usability.
- Legacy logging internals are kept in `include/corekit/legacy/log_manager_legacy.hpp`.
