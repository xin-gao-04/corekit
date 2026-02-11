# Industry Matrix

## Goal
Use mature industrial implementations as references before each implementation step.

## Logging
| Library | Strength | Weakness | Borrowed ideas |
|---|---|---|---|
| glog | Proven stability, low friction in C++ services | API is macro-centric | keep glog backend hidden behind interface |
| spdlog | Modern formatting and sink extension | Header-heavy footprint | keep sink style extensibility |
| Boost.Log | Feature rich and flexible filters | high complexity | avoid exposing complex template API |

## IPC
| Library | Strength | Weakness | Borrowed ideas |
|---|---|---|---|
| Boost.Interprocess | robust shared-memory primitives | API complexity | use fixed-size shared segment + explicit lifecycle |
| iceoryx | zero-copy and deterministic memory model | integration overhead | ring-buffer style transport with explicit ownership |
| ZeroMQ (inproc/ipc) | easy socket model | not shared-memory first | keep simple send/recv semantics |

## Task Runtime
| Library | Strength | Weakness | Borrowed ideas |
|---|---|---|---|
| oneTBB | industrial-grade scheduler | additional runtime dependency | executor abstraction + parallel_for semantics |
| folly executors | rich async model | heavy ecosystem dependency | explicit execution context objects |
| Taskflow | concise task graph API | C++20 baseline for latest versions | graph naming and dependency readability |

## Concurrent Containers
| Library | Strength | Weakness | Borrowed ideas |
|---|---|---|---|
| oneTBB containers | mature and tuned | requires TBB runtime | stable API patterns for map/queue |
| Folly queues/maps | high-performance options | portability and build complexity | non-blocking try APIs |
| moodycamel queue | simple and fast MPMC queue | feature scope focused on queue | lightweight try_push/try_pop UX |

## Memory
| Library | Strength | Weakness | Borrowed ideas |
|---|---|---|---|
| mimalloc | strong multithread allocation performance | optional replacement complexity | optional backend switching |
| jemalloc | battle-tested at scale | tuning complexity | metrics-oriented allocator configs |
| tbbmalloc | close integration with oneTBB | runtime coupling | keep as backend option only |

## Decision Notes
- Public API prioritizes readability and explicit status codes.
- Dynamic library boundary uses factory functions and pure virtual interfaces.
- IPC V1 targets shared-memory ring queue with non-blocking semantics.
