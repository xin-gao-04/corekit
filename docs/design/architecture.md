# Architecture

## Overview
`liblogkit` is split into interface headers and backend implementations.

- `include/liblogkit/...`: public API only.
- `src/...`: backend implementation details.
- `src/api/c_api.cpp`: DLL factory export boundary.

## Layering
1. API layer: pure virtual interfaces and shared status/version types.
2. Adapter layer: maps API interfaces to concrete backend implementations.
3. Backend layer: glog backend, shared-memory IPC backend.

## Design Rules
- No backend headers leaked to application code.
- All public APIs are comment-first and caller-oriented.
- Runtime checks for API version and input validation are mandatory.

## Implementation Sequence
1. Freeze interfaces.
2. Freeze design docs and diagrams.
3. Implement and validate module by module.
