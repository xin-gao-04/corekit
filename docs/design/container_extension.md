# Container Extension Notes (Phase-Next)

## Added Interfaces
- `IConcurrentSet<K>`: thread-safe set operations (`Insert`, `Erase`, `Contains`, `Snapshot`).
- `IRingBuffer<T>`: bounded ring buffer operations (`TryPush`, `TryPop`, `TryPeek`).

## Added Implementations
- `BasicConcurrentSetImpl<K>` in `src/concurrent/basic_set_impl.hpp`
- `BasicRingBufferImpl<T>` in `src/concurrent/basic_ring_buffer_impl.hpp`

## Memory Strategy Integration
- Both implementations allocate internal storage with `GlobalStlAllocator`.
- The allocator backend is controlled by `GlobalAllocator` and `config/corekit.json`.

## Why These Two Containers
- `Set`: frequently used for dedupe/state tracking in simulation scheduling and IPC routing.
- `RingBuffer`: common bounded buffer pattern for telemetry, staged pipelines, and lock-step loops.

## Follow-up Candidates
- Blocking queue (`WaitPush` / `WaitPop`) for producer-consumer pipelines.
- Sharded map/set variants for lower lock contention.
- Stable-handle vector/pool for entity-style simulation objects.
