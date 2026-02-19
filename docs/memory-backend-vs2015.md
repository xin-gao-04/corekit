# Memory Backend Integration Notes (VS2015)

## Goal
- Build `system + mimalloc + tbb` backends on `Visual Studio 2015`.
- Keep dependency sources in local `3party/` directory.
- Keep tests and perf benchmark runnable without backend `SKIP` in full-backend build.

## What changed
- CMake switched to local third-party layout:
  - `3party/glog`
  - `3party/mimalloc`
  - `3party/oneTBB`
- `project(corekit LANGUAGES C CXX)` enabled to support C-based dependency build flow.
- Removed online-fetch dependency path from main build flow and use `add_subdirectory` for local sources.
- TBB backend link fix:
  - link `TBB::tbbmalloc` + `TBB::tbb` (or local equivalents), preventing `tbb12.lib` link errors.
- mimalloc VS2015 compatibility:
  - patched local `3party/mimalloc/CMakeLists.txt` to avoid forcing `MI_USE_CXX=ON` on MSVC 1900.
  - this avoids the known `arena.c` atomic pointer operator ambiguity on VS2015 C++ path.

## Test updates
- `tests/interface_tests.cpp` was updated to remove fragile assumptions tied to backend compile-time availability and global allocator runtime state.
- JSON config test now validates:
  - valid config path for system backend
  - invalid backend string handling (`kInvalidArgument`)
  - stable file-based configure path
- fallback strictness test now validates unsupported backend behavior via invalid enum backend values.

## Verified commands
```powershell
cmake -S . -B build_vs2015_local -G "Visual Studio 14 2015 Win64" `
  -DCOREKIT_BUILD_EXAMPLES=OFF `
  -DCOREKIT_BUILD_TESTS=ON `
  -DCOREKIT_ENABLE_MIMALLOC_BACKEND=ON `
  -DCOREKIT_ENABLE_TBBMALLOC_BACKEND=ON

cmake --build build_vs2015_local --config Release --target corekit interface_tests corekit_legacy_tests memory_perf_compare

$env:PATH='D:\code\corekit\build_vs2015_local\Release;D:\code\corekit\build_vs2015_local\msvc_19.0_cxx14_64_md_release;'+$env:PATH
ctest --test-dir build_vs2015_local -C Release --output-on-failure
D:\code\corekit\build_vs2015_local\Release\memory_perf_compare.exe 300000
```

## Current benchmark sample (300000 iterations)
- `new_delete`: 36,575,552 ops/s
- `object_pool`: 2,695,333 ops/s
- `global_allocator[system]`: 3,931,549 ops/s
- `global_allocator[mimalloc]`: 4,040,861 ops/s
- `global_allocator[tbb]`: 4,758,219 ops/s

## Review focus suggestions
- GlobalAllocator uses a single process-global state; test isolation can still be sensitive to ordering.
- TBB build warnings under VS2015 are currently non-fatal (`C4800`, `C4592`) and should be tracked separately for warning policy hardening.
- If repository size is a concern, dependency source pinning strategy should be revisited (vendor vs mirror/submodule policy).

## Additional hardening in this iteration
- `BasicObjectPool` exception-safety hardening:
  - reserve/acquire/release bookkeeping paths now guard container insertion failures and return status instead of leaking/throwing through API boundaries.
- `GlobalAllocator` backend introspection APIs added:
  - `BackendDisplayName(AllocBackend)`
  - `IsBackendEnabled(AllocBackend)`
- Tests expanded:
  - `global_allocator_backend_introspection`

## Notes from code review
- `GlobalAllocator` currently serializes allocate/deallocate/configure under one mutex by design to keep backend-switch safety and deallocation routing correctness; this is safe but can limit high-concurrency allocation throughput. A later optimization can decouple this with pointer-to-backend ownership tracking.
