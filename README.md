# corekit

A dedicated C++ utility toolkit for simulation systems, delivered as a dynamic library plus interface headers.

## What is implemented in this stage
- Interface-first API (`pure virtual` classes) under `include/corekit`.
- DLL factory boundary (`extern "C"`) for ABI-stable object creation.
- Logging interface adapter over existing `glog`-based backend.
- IPC v1 interface and Windows shared-memory ring-buffer implementation.
- Allocator (`SystemAllocator`), executor (`ThreadPoolExecutor`), and task graph (`SimpleTaskGraph`) concrete implementations.
- Design docs and diagrams under `docs/`.
- Phase-1 freeze report: `docs/api/phase1_interface_freeze.md`.
- Phase-2 design freeze: `docs/design/phase2_design_freeze.md`.
- Phase-3 implementation report: `docs/design/phase3_implementation_report.md`.

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

## IPC usage (v1)
- Server creates a named channel via `OpenServer`.
- Client connects via `OpenClient`.
- Data path uses `TrySend` / `TryRecv` (non-blocking).

## Public headers
- `include/corekit/corekit.hpp`
- `include/corekit/log/ilog_manager.hpp`
- `include/corekit/ipc/i_channel.hpp`
- `include/corekit/api/factory.hpp`

## Notes
- Current IPC backend implementation is Windows-first (`CreateFileMapping` based).
- API comments are attached directly to virtual methods, focused on usability.
- Legacy `include/corekit/legacy/log_manager_legacy.hpp` is kept for compatibility.



