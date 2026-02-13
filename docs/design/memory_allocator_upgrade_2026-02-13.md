# Memory Allocator Upgrade Report (2026-02-13)

## Scope
This iteration focused on making memory behavior reviewable and measurable:
1. observability-first allocator interfaces,
2. safer backend switch semantics,
3. expanded tests that prove contracts and regression guards,
4. optional third-party backend integration points.

## Design Rationale
### Why observability first
The project previously exposed backend selection but not enough runtime evidence to prove which backend was actually used or how it behaved under load. We added capability/stats APIs to close this gap before introducing more backend complexity.

### Why switch guard
Switching allocator implementation while live memory exists can cause cross-allocator free hazards. We block backend switch (`kWouldBlock`) when `bytes_in_use != 0`.

### Why optional third-party backend gates
Different build environments have different dependency policies. Backend support is now explicit via CMake options and does not burden default builds.

## Implemented Changes
### Public interfaces
- `include/corekit/memory/iallocator.hpp`
  - Added `AllocatorCaps` and `AllocatorStats`.
  - Added virtual methods: `BackendName`, `Caps`, `Stats`, `ResetStats`.
- `include/corekit/memory/i_global_allocator.hpp`
  - Added static methods: `CurrentBackendName`, `CurrentCaps`, `CurrentStats`, `ResetCurrentStats`.

### Runtime behavior
- `src/memory/system_allocator.cpp/.hpp`
  - Added thread-safe counters: alloc/free/fail/live-bytes/peak-bytes.
  - Added pointer-size tracking to compute live bytes.
- `src/memory/global_allocator.cpp`
  - Added allocator creation path by backend.
  - Added switch guard on live allocations.
  - Preserved strict/fallback semantics.

### Optional backend integration points
- Added CMake options:
  - `COREKIT_ENABLE_MIMALLOC_BACKEND`
  - `COREKIT_ENABLE_TBBMALLOC_BACKEND`
- Added optional allocator implementations:
  - `src/memory/mimalloc_allocator.hpp/.cpp`
  - `src/memory/tbb_allocator.hpp/.cpp`

## Test Matrix Added/Extended
### Contract tests
- invalid arg + memory hex code consistency
- object pool capacity/clear behavior
- object pool memory-module error code mapping
- global allocator observability API correctness
- backend switch while in-use (`kWouldBlock`) + post-release fallback behavior

### Concurrency tests
- object pool concurrent acquire/release
- global allocator concurrent configure + allocate/deallocate

## Validation Executed
- Build targets:
  - `corekit`
  - `interface_tests`
  - `corekit_legacy_tests`
- Runtime tests:
  - `interface_tests.exe` all pass
  - `corekit_legacy_tests.exe` all pass

## Known Limitations / Next Iteration
1. Third-party backend source files are integrated behind build flags but not enabled by default.
2. Cross-backend switch is guarded, but a future iteration can add stronger ownership tagging for forensic diagnostics.
3. Benchmark executable/report generation should be added as a separate `bench` target for repeatable perf review.

## Reviewer Checklist
1. Verify new API additions are backward-compatible for current consumers.
2. Verify switch-guard semantics align with operational expectations.
3. Verify stats are monotonic and thread-safe under stress tests.
4. Decide whether to enable mimalloc/tbbmalloc in CI matrix.
