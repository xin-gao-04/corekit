# Decision Log

## 2026-02-11
- Adopted interface-first workflow: API headers before implementation.
- Public API uses pure virtual classes and DLL factory exports.
- Chose status-code-based error model instead of exceptions across DLL boundary.
- IPC V1 selected as shared-memory ring queue with non-blocking semantics.
- Queue-full default behavior: return `kWouldBlock` and count drop/backpressure metrics.
- Scope excluded frequency-domain computation modules for this phase.
