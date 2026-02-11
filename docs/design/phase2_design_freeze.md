# Phase 2 Design Freeze

## Objective
Freeze implementation-level design details after API freeze and before full implementation rollout.

## Frozen Decisions
1. Allocator implementation starts with system allocator as default backend.
2. Executor uses internal thread pool with non-throwing API surface.
3. Task graph v1 uses deterministic DAG execution with cycle detection.
4. IPC v1 remains shared-memory ring queue with non-blocking semantics.

## Data Flow
- API factory creates interface objects.
- Interface methods validate input and return `Status/Result`.
- Backend implementation performs work and keeps ABI hidden from users.

## Failure Semantics
- Invalid arguments => `kInvalidArgument`
- Missing initialization/resource => `kNotInitialized`/`kNotFound`
- Temporary unavailable => `kWouldBlock`
- Unsupported backend => `kUnsupported`

## Maintainability Rules
- Keep one class per file pair (`.hpp/.cpp`).
- Keep exported C API thin; no business logic in C layer.
- Use deterministic behavior in tests.
