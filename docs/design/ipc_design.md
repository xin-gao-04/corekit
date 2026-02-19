# IPC Design (V3)

## Scope
- Local machine inter-process communication.
- Shared memory variable-frame byte ring (Windows backend).
- Non-blocking APIs (`TrySend`, `TryRecv`).
- Redis-inspired staged send path (local outbox + budgeted flush).

## Shared Memory Layout
- Header (`SharedHeader`):
  - magic/version
  - logical `capacity` and `message_max_bytes`
  - `ring_bytes` (rounded to power-of-two) and `ring_mask`
  - `read_index` / `write_index` (byte offsets, monotonic)
  - shared counters (`send_ok`/`recv_ok`/`dropped_when_full`)
- Ring payload area:
  - variable frame: `FrameHeader{size,reserved} + payload + padding(8-byte aligned)`

## Why V3
- V1 fixed-slot ring wastes memory when message sizes vary.
- V3 uses variable frames while preserving FIFO and non-blocking API.
- Index modulo is optimized by `ring_mask` (`offset = index & mask`).

## Send/Recv Pipeline
- `TrySend`:
  1) enqueue to local outbox
  2) `ProcessIoOnce(write_budget)` flushes pending messages to shared ring
- `TryRecv`:
  1) `ProcessIoOnce(1)` helps forward pending writes
  2) parse one frame from shared ring

This mirrors Redis's "buffer first, flush in event loop" approach.

## Wrap-around Strategy
- Writer never writes a frame across ring end.
- If tail contiguous bytes are insufficient, writer advances `write_index` to ring start and writes there.
- Reader applies the same rule for `read_index` when tail bytes are too small for a frame header.

## Backpressure
- Shared ring full: flush stops with `kWouldBlock`, message stays in local outbox.
- Local outbox full:
  - return `kWouldBlock`
  - if `drop_when_full=true`, increment `dropped_when_full`.

## Concurrency Contract
- Guaranteed for SPSC (single-producer + single-consumer).
- MPMC requires external locking or a dedicated algorithm change.

## Validation and Safety
- On open client, verify `magic/version` and power-of-two `ring_bytes`.
- On recv, verify frame size does not exceed `message_max_bytes`.
- Incomplete frame is treated as temporary unavailability (`kWouldBlock`).

## Design References
- Redis-inspired notes: `docs/design/ipc_redis_inspired_design.md`
- Redis source references:
  - https://github.com/redis/redis/blob/unstable/src/networking.c
  - https://github.com/redis/redis/blob/unstable/src/ae.c

## Future Enhancements
- Blocking mode with timeout.
- Explicit `Flush/Poll` APIs and outbox observability metrics.
- MPMC-safe ring strategy (sequence-based slots).
- Cross-platform parity implementation for Linux/macOS.
