# 3party

This directory stores vendored third-party dependencies when needed.

Current status:
- Vendored `concurrentqueue.h` from moodycamel (single-header queue).
- Core container/object-pool implementations are also available in std-based versions.

Vendored files
- `3party/concurrentqueue.h` (source: https://github.com/cameron314/concurrentqueue)

Future candidates to vendor here:
- oneTBB
- mimalloc
