# Decision Log

## 2026-02-11
- Adopted interface-first workflow: API headers before implementation.
- Public API uses pure virtual classes and DLL factory exports.
- Chose status-code-based error model instead of exceptions across DLL boundary.
- IPC V1 selected as shared-memory ring queue with non-blocking semantics.
- Queue-full default behavior: return `kWouldBlock` and count drop/backpressure metrics.
- Scope excluded frequency-domain computation modules for this phase.

## 2026-02-13
- Standardized module-oriented hex error code model and added memory-module status wiring.
- Expanded memory tests to include invalid-argument paths, object-pool concurrency/contract checks, and global allocator concurrent configure+allocate checks.
- Added allocator observability contract (`BackendName`, `Caps`, `Stats`, `ResetStats`) and global observability entrypoints.
- Added backend-switch safety guard: switch is rejected with `kWouldBlock` when live allocations exist.
- Introduced optional build gates for third-party memory backends (`COREKIT_ENABLE_MIMALLOC_BACKEND`, `COREKIT_ENABLE_TBBMALLOC_BACKEND`) with strict/fallback semantics retained.
