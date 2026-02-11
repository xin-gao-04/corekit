# corekit

A dedicated C++ utility toolkit for engineering and simulation workloads, delivered as a dynamic library plus interface headers.

## What is implemented
- Interface-first API (`pure virtual` classes) under `include/corekit`.
- DLL factory boundary (`extern "C"`) for ABI-stable object creation.
- Logging interface adapter over legacy glog backend.
- IPC v1 interface and Windows shared-memory ring-buffer implementation.
- Allocator (`SystemAllocator`), executor (`ThreadPoolExecutor`), and task graph (`SimpleTaskGraph`) concrete implementations.
- Header-only usable containers and pool implementations:
  - `corekit::concurrent::BasicMutexQueue<T>`
  - `corekit::concurrent::BasicConcurrentMap<K, V>`
  - `corekit::memory::BasicObjectPool<T>`
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

## Header-only utility usage
```cpp
#include "corekit/corekit.hpp"

corekit::concurrent::BasicMutexQueue<int> q(16);
q.TryPush(7);
auto r = q.TryPop();
```

## IPC usage (v1)
- Server creates a named channel via `OpenServer`.
- Client connects via `OpenClient`.
- Data path uses `TrySend` / `TryRecv` (non-blocking).

## Public headers
- `include/corekit/corekit.hpp`
- `include/corekit/log/ilog_manager.hpp`
- `include/corekit/ipc/i_channel.hpp`
- `include/corekit/api/factory.hpp`
- `include/corekit/concurrent/basic_queue.hpp`
- `include/corekit/concurrent/basic_map.hpp`
- `include/corekit/memory/basic_object_pool.hpp`

## Notes
- Current IPC backend implementation is Windows-first (`CreateFileMapping` based).
- API comments are attached directly to virtual methods, focused on usability.
- Legacy logging internals are kept in `include/corekit/legacy/log_manager_legacy.hpp`.
