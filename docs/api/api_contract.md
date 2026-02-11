# API Contract

## Versioning
- Runtime API version: `corekit_get_api_version()`
- All C++ interfaces expose `ApiVersion()`.

## Lifetime
- Objects are created by exported factory methods.
- Caller must destroy via matching destroy function or `Release()`.

## Error Model
- No exception across DLL boundary.
- `Status` and `Result<T>` are mandatory return wrappers.

## Threading Model
- `ILogManager`: thread-safe for Init/Reload/Log/Shutdown.
- `IChannel`: process-safe by shared memory; single producer + single consumer semantics in v1.

## Module Contracts
### ILogManager
- `Init`: idempotent startup for logging backend.
- `Reload`: configuration hot reload.
- `Log`: severity-aware logging entry.
- `CurrentOptions`: snapshot of effective config.
- `Shutdown`: graceful backend shutdown.

### IChannel
- `OpenServer`: create and initialize named channel.
- `OpenClient`: attach to existing channel.
- `TrySend`: non-blocking send.
- `TryRecv`: non-blocking receive.
- `GetStats`: runtime observability counters.
- `Close`: release process-local handles.

### IAllocator
- `SetBackend`: switch allocator backend for later allocations.
- `Allocate/Deallocate`: allocation pair with explicit status code.
- Current implementation: system allocator backend is available.

### IExecutor
- `Submit`: enqueue a task.
- `ParallelFor`: parallel range execution.
- `WaitAll`: wait for submitted tasks.
- Current implementation: thread-pool executor backend is available.

### ITaskGraph
- `AddTask`: add task node and return task id.
- `AddDependency`: build DAG dependency edge.
- `Run`: execute graph.
- Current implementation: deterministic DAG executor backend is available.

### IObjectPool / IConcurrentMap / IQueue
- Pool, concurrent map, and queue interfaces are included in headers and frozen for ABI.
- Queue may have concrete implementation earlier than pool/map depending on module priority.

## Compatibility Rules
- Do not remove existing virtual methods.
- Additive changes require version bump.
- ABI changes require major version increment.


