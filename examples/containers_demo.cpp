#include "corekit/corekit.hpp"

#include <cstdio>

int main() {
  corekit::concurrent::BasicMutexQueue<int> q(8);
  q.TryPush(10);
  q.TryPush(20);
  corekit::api::Result<int> a = q.TryPop();
  corekit::api::Result<int> b = q.TryPop();
  if (a.ok() && b.ok()) {
    std::printf("queue: %d, %d\n", a.value(), b.value());
  }

  corekit::concurrent::MoodycamelQueue<int> mq(32);
  mq.TryPush(100);
  mq.TryPush(200);
  corekit::api::Result<int> mqa = mq.TryPop();
  corekit::api::Result<int> mqb = mq.TryPop();
  if (mqa.ok() && mqb.ok()) {
    std::printf("moodycamel queue: %d, %d\n", mqa.value(), mqb.value());
  }

  corekit::concurrent::BasicConcurrentMap<int, const char*> m;
  m.Upsert(1, "alpha");
  corekit::api::Result<const char*> r = m.Find(1);
  if (r.ok()) {
    std::printf("map: key=1 value=%s\n", r.value());
  }

  corekit::memory::BasicObjectPool<int> pool(4);
  pool.Reserve(2);
  corekit::api::Result<int*> p = pool.Acquire();
  if (p.ok()) {
    *p.value() = 42;
    std::printf("pool: %d\n", *p.value());
    pool.ReleaseObject(p.value());
  }

  return 0;
}
