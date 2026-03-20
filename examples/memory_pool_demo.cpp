/// memory_pool_demo.cpp — 内存池 (Slab Pool) 使用示例
///
/// 展示 corekit 分层内存池的核心用法：
///   - 系统级 slab 池创建与配置
///   - 子池（子系统隔离）
///   - 小对象高频分配（slab 加速）
///   - 大对象直通分配
///   - Guard bytes 溢出检测
///   - 对象池 + 内存池组合
///   - 统计与诊断

#include "corekit/corekit.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

// ============================================================================
// 1. 基础用法 — 创建池，分配/释放
// ============================================================================
static void demo_basic() {
  std::printf("\n=== 1. Basic Slab Pool ===\n");

  corekit::memory::IMemoryPool* pool = corekit_create_memory_pool();

  corekit::memory::PoolConfig cfg;
  cfg.name = "demo_basic";
  cfg.block_size = 65536;  // 64KB per block
  pool->Init(cfg);

  // Small allocation — served from slab cache.
  void* p1 = pool->Alloc(64);
  void* p2 = pool->Alloc(128);
  void* p3 = pool->Alloc(256);
  std::printf("  Alloc 64B:  %p\n", p1);
  std::printf("  Alloc 128B: %p\n", p2);
  std::printf("  Alloc 256B: %p\n", p3);

  // Write some data.
  std::memset(p1, 0xAA, 64);
  std::memset(p2, 0xBB, 128);

  // Free.
  pool->Free(p1);
  pool->Free(p2);
  pool->Free(p3);

  // Check stats.
  corekit::memory::PoolStats stats = pool->Stats();
  std::printf("  Stats: alloc=%llu, free=%llu, blocks=%llu\n",
              (unsigned long long)stats.alloc_count,
              (unsigned long long)stats.free_count,
              (unsigned long long)stats.block_count);

  corekit_destroy_memory_pool(pool);
}

// ============================================================================
// 2. 子池 — 子系统内存隔离
// ============================================================================
static void demo_child_pool() {
  std::printf("\n=== 2. Child Pools (Subsystem Isolation) ===\n");

  corekit::memory::IMemoryPool* system_pool = corekit_create_memory_pool();

  corekit::memory::PoolConfig sys_cfg;
  sys_cfg.name = "system";
  sys_cfg.max_bytes = 1024 * 1024;  // 1MB total budget
  system_pool->Init(sys_cfg);

  // Create child pools for different subsystems.
  corekit::memory::PoolConfig physics_cfg;
  physics_cfg.name = "physics";
  physics_cfg.max_bytes = 512 * 1024;  // 512KB budget

  corekit::memory::PoolConfig render_cfg;
  render_cfg.name = "render";
  render_cfg.max_bytes = 256 * 1024;  // 256KB budget

  corekit::api::Result<corekit::memory::IMemoryPool*> physics =
      system_pool->CreateChild(physics_cfg);
  corekit::api::Result<corekit::memory::IMemoryPool*> render =
      system_pool->CreateChild(render_cfg);

  if (physics.ok() && render.ok()) {
    // Each subsystem allocates independently.
    void* phys_data = physics.value()->Alloc(1024);
    void* rend_data = render.value()->Alloc(2048);

    std::printf("  Physics pool alloc 1KB: %p\n", phys_data);
    std::printf("  Render  pool alloc 2KB: %p\n", rend_data);

    // Stats show per-pool usage.
    corekit::memory::PoolStats phys_stats = physics.value()->Stats();
    corekit::memory::PoolStats rend_stats = render.value()->Stats();
    corekit::memory::PoolStats sys_stats = system_pool->Stats();

    std::printf("  Physics: in_use=%llu, limit=%llu\n",
                (unsigned long long)phys_stats.bytes_in_use,
                (unsigned long long)phys_stats.bytes_limit);
    std::printf("  Render:  in_use=%llu, limit=%llu\n",
                (unsigned long long)rend_stats.bytes_in_use,
                (unsigned long long)rend_stats.bytes_limit);
    std::printf("  System:  in_use=%llu (child usage counted)\n",
                (unsigned long long)sys_stats.bytes_in_use);

    physics.value()->Free(phys_data);
    render.value()->Free(rend_data);
  }

  // Children are released when parent is destroyed.
  corekit_destroy_memory_pool(system_pool);
}

// ============================================================================
// 3. Guard Bytes — 溢出检测
// ============================================================================
static void demo_guard_bytes() {
  std::printf("\n=== 3. Guard Bytes (Overflow Detection) ===\n");

  corekit::memory::IMemoryPool* pool = corekit_create_memory_pool();

  corekit::memory::PoolConfig cfg;
  cfg.name = "guarded";
  cfg.enable_guard_bytes = true;  // Enable overflow detection!
  pool->Init(cfg);

  // Allocate with guard bytes enabled.
  void* p = pool->Alloc(100);
  std::printf("  Allocated 100 bytes with guard: %p\n", p);

  // Normal write — within bounds.
  std::memset(p, 0x42, 100);

  // Free — guard check happens automatically.
  pool->Free(p);
  std::printf("  Free succeeded — no overflow detected.\n");

  // Note: If you wrote beyond 100 bytes (e.g., memset(p, 0x42, 110)),
  // the guard validation in Free() would detect the overflow.
  // In production, you could hook this to logging/assertions.

  corekit_destroy_memory_pool(pool);
}

// ============================================================================
// 4. Slab 性能对比 — slab pool vs 直接 malloc
// ============================================================================
static void demo_performance() {
  std::printf("\n=== 4. Slab Pool Performance ===\n");

  static const int ITERATIONS = 100000;
  static const std::size_t ALLOC_SIZE = 64;

  // --- Slab pool ---
  corekit::memory::IMemoryPool* pool = corekit_create_memory_pool();
  corekit::memory::PoolConfig cfg;
  cfg.name = "perf";
  cfg.block_size = 65536;
  pool->Init(cfg);

  std::vector<void*> ptrs(ITERATIONS);

  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    ptrs[i] = pool->Alloc(ALLOC_SIZE);
  }
  for (int i = 0; i < ITERATIONS; ++i) {
    pool->Free(ptrs[i]);
  }
  auto t1 = std::chrono::high_resolution_clock::now();

  double slab_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  corekit::memory::PoolStats stats = pool->Stats();
  std::printf("  Slab pool: %d alloc+free in %.2f ms\n", ITERATIONS, slab_ms);
  std::printf("    slab_hits=%llu, slab_misses=%llu, blocks=%llu\n",
              (unsigned long long)stats.slab_hits,
              (unsigned long long)stats.slab_misses,
              (unsigned long long)stats.block_count);

  corekit_destroy_memory_pool(pool);

  // --- Direct malloc/free ---
  t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    ptrs[i] = std::malloc(ALLOC_SIZE);
  }
  for (int i = 0; i < ITERATIONS; ++i) {
    std::free(ptrs[i]);
  }
  t1 = std::chrono::high_resolution_clock::now();

  double malloc_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::printf("  malloc/free: %d alloc+free in %.2f ms\n", ITERATIONS, malloc_ms);

  if (malloc_ms > 0) {
    std::printf("  Speedup: %.1fx\n", malloc_ms / slab_ms);
  }
}

// ============================================================================
// 5. 大对象分配 — 超过 slab 上限 (>4096) 自动直通
// ============================================================================
static void demo_large_alloc() {
  std::printf("\n=== 5. Large Object Allocation ===\n");

  corekit::memory::IMemoryPool* pool = corekit_create_memory_pool();
  corekit::memory::PoolConfig cfg;
  cfg.name = "large";
  pool->Init(cfg);

  // 小对象走 slab.
  void* small = pool->Alloc(256);
  std::printf("  Small (256B): %p — from slab\n", small);

  // 大对象直通底层分配器.
  void* large = pool->Alloc(8192);
  std::printf("  Large (8KB):  %p — direct alloc\n", large);

  void* huge = pool->Alloc(1024 * 1024);
  std::printf("  Huge  (1MB):  %p — direct alloc\n", huge);

  corekit::memory::PoolStats stats = pool->Stats();
  std::printf("  Stats: alloc=%llu, bytes_in_use=%llu\n",
              (unsigned long long)stats.alloc_count,
              (unsigned long long)stats.bytes_in_use);

  pool->Free(small);
  pool->Free(large);
  pool->Free(huge);

  corekit_destroy_memory_pool(pool);
}

// ============================================================================
// 6. Shrink — 释放空闲块，回收碎片
// ============================================================================
static void demo_shrink() {
  std::printf("\n=== 6. Shrink (Release Idle Blocks) ===\n");

  corekit::memory::IMemoryPool* pool = corekit_create_memory_pool();
  corekit::memory::PoolConfig cfg;
  cfg.name = "shrink";
  cfg.block_size = 4096;  // Small blocks for demo.
  pool->Init(cfg);

  // Allocate many small objects, creating multiple blocks.
  std::vector<void*> ptrs;
  for (int i = 0; i < 200; ++i) {
    ptrs.push_back(pool->Alloc(32));
  }

  corekit::memory::PoolStats before = pool->Stats();
  std::printf("  Before free: blocks=%llu, in_use=%llu\n",
              (unsigned long long)before.block_count,
              (unsigned long long)before.bytes_in_use);

  // Free all.
  for (std::size_t i = 0; i < ptrs.size(); ++i) {
    pool->Free(ptrs[i]);
  }

  corekit::memory::PoolStats after_free = pool->Stats();
  std::printf("  After free:  blocks=%llu, in_use=%llu (blocks still cached)\n",
              (unsigned long long)after_free.block_count,
              (unsigned long long)after_free.bytes_in_use);

  // Shrink — release idle blocks back to system.
  pool->Shrink();

  corekit::memory::PoolStats after_shrink = pool->Stats();
  std::printf("  After shrink: blocks=%llu\n",
              (unsigned long long)after_shrink.block_count);

  corekit_destroy_memory_pool(pool);
}

// ============================================================================
int main() {
  std::printf("====== Corekit Memory Pool Demo ======\n");

  demo_basic();
  demo_child_pool();
  demo_guard_bytes();
  demo_performance();
  demo_large_alloc();
  demo_shrink();

  std::printf("\n====== All memory demos completed ======\n");
  return 0;
}
