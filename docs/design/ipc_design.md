# IPC Design (V1)

## Scope
- Local machine inter-process communication.
- Shared memory ring queue.
- Non-blocking APIs (`TrySend`, `TryRecv`).
- Fixed-size slot payload (`message_max_bytes`).

## Shared Memory Layout
- Header:
  - magic/version
  - capacity/message_max_bytes
  - read/write indexes
  - shared counters
- Slots:
  - `state` (0 empty, 1 full)
  - `size`
  - payload bytes

## Semantics
- Producer checks queue fullness via `write - read >= capacity`.
- Consumer checks availability via `read < write`.
- Full queue returns `kWouldBlock`; if configured, increments `dropped_when_full`.
- Empty queue returns `kWouldBlock`.

## Limits and Guarantees
- V1 guarantees single-producer + single-consumer behavior.
- Multi-producer or multi-consumer requires external locking.
- Message order is FIFO.

## Failure Handling
- Invalid open options -> `kInvalidArgument`.
- Client open before server -> `kNotFound`.
- Buffer too small on recv -> `kBufferTooSmall`.
- Close is idempotent.

## Future Enhancements
- Blocking mode with timeout.
- MPMC safe variant.
- Cross-platform parity implementation for Linux/macOS.
