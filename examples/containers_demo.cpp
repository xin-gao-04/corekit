// 并发容器示例
//
// 演示 corekit 内置并发容器的基本用法：
//   - BasicMutexQueue      : 互斥锁 FIFO 队列
//   - MoodycamelQueue      : 无锁高吞吐队列
//   - BasicConcurrentMap   : 并发哈希映射
//   - BasicConcurrentSet   : 并发哈希集合
//   - BasicRingBuffer      : 定长环形缓冲区
//   - BasicObjectPool      : 对象复用池

#include "corekit/concurrent/containers.hpp"

#include <cstdio>
#include <string>

// ── Queue ──────────────────────────────────────────────────────────────────

static void DemoQueue() {
  std::printf("=== BasicMutexQueue ===\n");

  // capacity=4：最多容纳 4 个元素，超出返回 kWouldBlock
  corekit::concurrent::BasicMutexQueue<int> q(4);
  q.TryPush(10);
  q.TryPush(20);
  q.TryPush(30);

  while (!q.IsEmpty()) {
    corekit::api::Result<int> r = q.TryPop();
    if (r.ok()) std::printf("  pop: %d\n", r.value());
  }

  std::printf("=== MoodycamelQueue ===\n");
  corekit::concurrent::MoodycamelQueue<int> mq(32);
  mq.TryPush(100);
  mq.TryPush(200);
  corekit::api::Result<int> a = mq.TryPop();
  corekit::api::Result<int> b = mq.TryPop();
  if (a.ok() && b.ok()) {
    std::printf("  pop: %d, %d\n", a.value(), b.value());
  }
}

// ── Map ───────────────────────────────────────────────────────────────────

static void DemoMap() {
  std::printf("=== BasicConcurrentMap ===\n");

  corekit::concurrent::BasicConcurrentMap<int, std::string> m;
  m.Upsert(1, "alpha");
  m.Upsert(2, "beta");

  corekit::api::Result<std::string> r = m.Find(1);
  if (r.ok()) std::printf("  find(1) = %s\n", r.value().c_str());

  corekit::api::Status st = m.InsertIfAbsent(1, "ignored");
  std::printf("  InsertIfAbsent(1): %s\n", st.ok() ? "inserted" : "already exists");

  m.Erase(2);
  std::printf("  contains(2) after erase: %s\n", m.Contains(2) ? "yes" : "no");
  std::printf("  approx size: %zu\n", m.ApproxSize());
}

// ── Set ───────────────────────────────────────────────────────────────────

static void DemoSet() {
  std::printf("=== BasicConcurrentSet ===\n");

  corekit::concurrent::BasicConcurrentSet<int> s;
  s.Insert(7);
  s.Insert(8);
  s.Insert(9);

  std::printf("  contains(7): %s\n", s.Contains(7) ? "yes" : "no");
  s.Erase(7);
  std::printf("  contains(7) after erase: %s\n", s.Contains(7) ? "yes" : "no");
  std::printf("  approx size: %zu\n", s.ApproxSize());
}

// ── RingBuffer ────────────────────────────────────────────────────────────

static void DemoRingBuffer() {
  std::printf("=== BasicRingBuffer ===\n");

  corekit::concurrent::BasicRingBuffer<int> rb(3);
  rb.TryPush(1);
  rb.TryPush(2);
  rb.TryPush(3);

  // 第 4 次 push 应当失败（返回 kWouldBlock）
  corekit::api::Status full_st = rb.TryPush(4);
  std::printf("  push when full: %s\n",
              full_st.ok() ? "ok (unexpected)" : "would_block (expected)");

  while (!rb.IsEmpty()) {
    corekit::api::Result<int> r = rb.TryPop();
    if (r.ok()) std::printf("  pop: %d\n", r.value());
  }
}

// ── ObjectPool ────────────────────────────────────────────────────────────

static void DemoObjectPool() {
  std::printf("=== BasicObjectPool ===\n");

  // max_cached=4：最多缓存 4 个回收对象
  corekit::memory::BasicObjectPool<int> pool(4);

  // 预热：预先分配 2 个对象，减少后续 Acquire 的分配延迟
  pool.Reserve(2);
  std::printf("  after Reserve(2): available=%zu, total=%zu\n",
              pool.Available(), pool.TotalAllocated());

  // 借出对象
  corekit::api::Result<int*> p1 = pool.Acquire();
  corekit::api::Result<int*> p2 = pool.Acquire();
  if (p1.ok()) *p1.value() = 100;
  if (p2.ok()) *p2.value() = 200;
  std::printf("  acquired: %d, %d\n",
              p1.ok() ? *p1.value() : -1,
              p2.ok() ? *p2.value() : -1);

  // 归还对象（对象内容不会被自动清零，下次使用前需重新初始化）
  if (p1.ok()) pool.ReleaseObject(p1.value());
  if (p2.ok()) pool.ReleaseObject(p2.value());
  std::printf("  after release: available=%zu\n", pool.Available());

  // 裁剪空闲缓存（保留 1 个）
  pool.Trim(1);
  std::printf("  after Trim(1): available=%zu\n", pool.Available());
}

// ── main ──────────────────────────────────────────────────────────────────

int main() {
  DemoQueue();
  DemoMap();
  DemoSet();
  DemoRingBuffer();
  DemoObjectPool();
  return 0;
}
