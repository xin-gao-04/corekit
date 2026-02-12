#include "corekit/corekit.hpp"
#include "src/concurrent/basic_map_impl.hpp"
#include "src/concurrent/basic_queue_impl.hpp"
#include "src/concurrent/basic_ring_buffer_impl.hpp"
#include "src/concurrent/basic_set_impl.hpp"
#include "src/concurrent/moodycamel_queue_impl.hpp"
#include "src/memory/basic_object_pool_impl.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

bool TestApiVersion() {
  return corekit_get_api_version() == corekit::api::kApiVersion;
}

bool TestFactoryLifecycle() {
  corekit::log::ILogManager* logger = corekit_create_log_manager();
  if (logger == NULL) return false;
  if (logger->ApiVersion() != corekit::api::kApiVersion) return false;
  corekit_destroy_log_manager(logger);

  corekit::ipc::IChannel* ch = corekit_create_ipc_channel();
  if (ch == NULL) return false;
  if (ch->ApiVersion() != corekit::api::kApiVersion) return false;
  corekit_destroy_ipc_channel(ch);

  corekit::memory::IAllocator* allocator = corekit_create_allocator();
  if (allocator == NULL) return false;
  if (allocator->ApiVersion() != corekit::api::kApiVersion) return false;
  corekit_destroy_allocator(allocator);

  corekit::task::IExecutor* executor = corekit_create_executor();
  if (executor == NULL) return false;
  if (executor->ApiVersion() != corekit::api::kApiVersion) return false;
  corekit_destroy_executor(executor);

  corekit::task::ITaskGraph* graph = corekit_create_task_graph();
  if (graph == NULL) return false;
  if (graph->ApiVersion() != corekit::api::kApiVersion) return false;
  corekit_destroy_task_graph(graph);
  return true;
}

bool TestAllocatorBasic() {
  corekit::memory::IAllocator* allocator = corekit_create_allocator();
  if (allocator == NULL) return false;
  corekit::api::Result<void*> alloc = allocator->Allocate(64, 16);
  if (!alloc.ok() || alloc.value() == NULL) return false;
  unsigned char* p = static_cast<unsigned char*>(alloc.value());
  for (int i = 0; i < 64; ++i) p[i] = static_cast<unsigned char>(i);
  corekit::api::Status st_alloc = allocator->Deallocate(alloc.value());
  corekit_destroy_allocator(allocator);
  if (!st_alloc.ok()) return false;

  return true;
}

void IncCounterTask(void* user_data) {
  std::atomic<int>* c = static_cast<std::atomic<int>*>(user_data);
  c->fetch_add(1, std::memory_order_relaxed);
}

bool TestExecutorSubmitAndWait() {
  corekit::task::IExecutor* executor = corekit_create_executor();
  if (executor == NULL) return false;
  std::atomic<int> counter(0);
  for (int i = 0; i < 100; ++i) {
    corekit::api::Status st = executor->Submit(&IncCounterTask, &counter);
    if (!st.ok()) return false;
  }
  corekit::api::Status st_exec = executor->WaitAll();
  corekit_destroy_executor(executor);
  if (!st_exec.ok()) return false;
  return counter.load(std::memory_order_relaxed) == 100;
}

void AddIndexTask(std::size_t index, void* user_data) {
  std::atomic<long long>* sum = static_cast<std::atomic<long long>*>(user_data);
  sum->fetch_add(static_cast<long long>(index), std::memory_order_relaxed);
}

bool TestExecutorParallelFor() {
  corekit::task::IExecutor* executor = corekit_create_executor();
  if (executor == NULL) return false;
  std::atomic<long long> sum(0);
  corekit::api::Status st = executor->ParallelFor(1, 101, 8, &AddIndexTask, &sum);
  corekit_destroy_executor(executor);
  if (!st.ok()) return false;
  return sum.load(std::memory_order_relaxed) == 5050;
}

struct SerialTaskCtx {
  std::atomic<int>* running;
  std::atomic<int>* max_running;
  std::atomic<int>* executed;
  int sleep_ms;
};

void SerialTask(void* user_data) {
  SerialTaskCtx* ctx = static_cast<SerialTaskCtx*>(user_data);
  const int now = ctx->running->fetch_add(1, std::memory_order_relaxed) + 1;

  int observed = ctx->max_running->load(std::memory_order_relaxed);
  while (observed < now &&
         !ctx->max_running->compare_exchange_weak(observed, now, std::memory_order_relaxed)) {
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(ctx->sleep_ms));
  ctx->executed->fetch_add(1, std::memory_order_relaxed);
  ctx->running->fetch_sub(1, std::memory_order_relaxed);
}

bool TestExecutorSubmitWithKeyAndCancel() {
  corekit::task::ExecutorOptions opt;
  opt.worker_count = 4;
  corekit::task::IExecutor* executor = corekit_create_executor_v2(&opt);
  if (executor == NULL) return false;

  std::atomic<int> running(0);
  std::atomic<int> max_running(0);
  std::atomic<int> executed(0);
  SerialTaskCtx ctx = {&running, &max_running, &executed, 80};

  corekit::api::Result<corekit::task::TaskId> t1 =
      executor->SubmitWithKey(99, &SerialTask, &ctx);
  corekit::api::Result<corekit::task::TaskId> t2 =
      executor->SubmitWithKey(99, &SerialTask, &ctx);
  if (!t1.ok() || !t2.ok()) return false;

  corekit::api::Status cancel = executor->TryCancel(t2.value());
  if (!cancel.ok()) return false;

  if (!executor->Wait(t1.value(), 0).ok()) return false;
  if (!executor->Wait(t2.value(), 0).ok()) return false;

  corekit::api::Result<corekit::task::ExecutorStats> stats = executor->QueryStats();
  corekit_destroy_executor(executor);
  if (!stats.ok()) return false;
  if (stats.value().canceled < 1) return false;
  if (executed.load(std::memory_order_relaxed) != 1) return false;
  return max_running.load(std::memory_order_relaxed) <= 1;
}

bool TestExecutorSubmitExSerialKey() {
  corekit::task::ExecutorOptions opt;
  opt.worker_count = 4;
  corekit::task::IExecutor* executor = corekit_create_executor_v2(&opt);
  if (executor == NULL) return false;

  std::atomic<int> running(0);
  std::atomic<int> max_running(0);
  std::atomic<int> executed(0);
  SerialTaskCtx ctx = {&running, &max_running, &executed, 60};

  corekit::task::TaskSubmitOptions aopt;
  aopt.serial_key = 12345;
  corekit::task::TaskSubmitOptions bopt;
  bopt.serial_key = 12345;

  corekit::api::Result<corekit::task::TaskId> a = executor->SubmitEx(&SerialTask, &ctx, aopt);
  corekit::api::Result<corekit::task::TaskId> b = executor->SubmitEx(&SerialTask, &ctx, bopt);
  if (!a.ok() || !b.ok()) return false;

  corekit::task::TaskId ids[2] = {a.value(), b.value()};
  if (!executor->WaitBatch(ids, 2, 0).ok()) return false;

  corekit_destroy_executor(executor);
  if (executed.load(std::memory_order_relaxed) != 2) return false;
  return max_running.load(std::memory_order_relaxed) <= 1;
}

bool TestExecutorWaitAllSubmittedBefore() {
  corekit::task::ExecutorOptions opt;
  opt.worker_count = 4;
  corekit::task::IExecutor* executor = corekit_create_executor_v2(&opt);
  if (executor == NULL) return false;

  std::atomic<int> done(0);
  auto work = [](void* p) {
    std::atomic<int>* d = static_cast<std::atomic<int>*>(p);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    d->fetch_add(1, std::memory_order_relaxed);
  };

  for (int i = 0; i < 20; ++i) {
    if (!executor->Submit(work, &done).ok()) return false;
  }

  if (!executor->WaitAllSubmittedBefore().ok()) return false;
  corekit::api::Result<corekit::task::ExecutorStats> stats = executor->QueryStats();
  corekit_destroy_executor(executor);
  if (!stats.ok()) return false;
  return done.load(std::memory_order_relaxed) == 20 &&
         stats.value().completed >= 20;
}

struct PriorityOrderCtx {
  std::vector<int>* order;
  std::mutex* order_mu;
  int value;
};

void PriorityProbeTask(void* user_data) {
  PriorityOrderCtx* ctx = static_cast<PriorityOrderCtx*>(user_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  std::lock_guard<std::mutex> lock(*ctx->order_mu);
  ctx->order->push_back(ctx->value);
}

struct BlockerCtx {
  std::atomic<bool>* release;
};

void BlockingTask(void* user_data) {
  BlockerCtx* ctx = static_cast<BlockerCtx*>(user_data);
  while (!ctx->release->load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool TestExecutorPriorityPolicy() {
  corekit::task::ExecutorOptions opt;
  opt.worker_count = 1;
  opt.policy = corekit::task::ExecutorPolicy::kPriority;
  corekit::task::IExecutor* executor = corekit_create_executor_v2(&opt);
  if (executor == NULL) return false;

  std::atomic<bool> release(false);
  std::vector<int> order;
  std::mutex order_mu;

  BlockerCtx blocker_ctx = {&release};
  PriorityOrderCtx low_ctx = {&order, &order_mu, 1};
  PriorityOrderCtx high_ctx = {&order, &order_mu, 2};

  if (!executor->Submit(&BlockingTask, &blocker_ctx).ok()) return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  corekit::task::TaskSubmitOptions low_opt;
  low_opt.priority = corekit::task::TaskPriority::kLow;
  corekit::task::TaskSubmitOptions high_opt;
  high_opt.priority = corekit::task::TaskPriority::kHigh;

  corekit::api::Result<corekit::task::TaskId> low =
      executor->SubmitEx(&PriorityProbeTask, &low_ctx, low_opt);
  corekit::api::Result<corekit::task::TaskId> high =
      executor->SubmitEx(&PriorityProbeTask, &high_ctx, high_opt);
  if (!low.ok() || !high.ok()) return false;

  release.store(true, std::memory_order_release);
  if (!executor->WaitAll().ok()) return false;
  corekit_destroy_executor(executor);

  if (order.size() != 2) return false;
  return order[0] == 2 && order[1] == 1;
}

struct GraphCheckCtx {
  std::atomic<int>* stage;
  std::atomic<int>* errors;
};

void TaskA(void* user_data) {
  GraphCheckCtx* c = static_cast<GraphCheckCtx*>(user_data);
  c->stage->store(1, std::memory_order_release);
}

void TaskB(void* user_data) {
  GraphCheckCtx* c = static_cast<GraphCheckCtx*>(user_data);
  if (c->stage->load(std::memory_order_acquire) < 1) {
    c->errors->fetch_add(1, std::memory_order_relaxed);
  }
  c->stage->store(2, std::memory_order_release);
}

void TaskC(void* user_data) {
  GraphCheckCtx* c = static_cast<GraphCheckCtx*>(user_data);
  if (c->stage->load(std::memory_order_acquire) < 2) {
    c->errors->fetch_add(1, std::memory_order_relaxed);
  }
}

bool TestTaskGraphDependency() {
  corekit::task::ITaskGraph* graph = corekit_create_task_graph();
  if (graph == NULL) return false;
  std::atomic<int> stage(0);
  std::atomic<int> errors(0);
  GraphCheckCtx ctx = {&stage, &errors};

  corekit::api::Result<std::uint64_t> a = graph->AddTask(&TaskA, &ctx);
  corekit::api::Result<std::uint64_t> b = graph->AddTask(&TaskB, &ctx);
  corekit::api::Result<std::uint64_t> c = graph->AddTask(&TaskC, &ctx);
  if (!a.ok() || !b.ok() || !c.ok()) return false;
  if (!graph->AddDependency(a.value(), b.value()).ok()) return false;
  if (!graph->AddDependency(b.value(), c.value()).ok()) return false;
  if (!graph->Run().ok()) return false;
  corekit_destroy_task_graph(graph);
  if (errors.load(std::memory_order_relaxed) != 0) return false;
  return true;
}

bool TestTaskGraphValidateAndRunWithExecutor() {
  corekit::task::ITaskGraph* graph = corekit_create_task_graph();
  corekit::task::IExecutor* executor = corekit_create_executor();
  if (graph == NULL || executor == NULL) return false;

  std::atomic<int> v(0);
  auto task_inc = [](void* user_data) {
    std::atomic<int>* p = static_cast<std::atomic<int>*>(user_data);
    p->fetch_add(1, std::memory_order_relaxed);
  };

  corekit::api::Result<std::uint64_t> a = graph->AddTask(task_inc, &v);
  corekit::api::Result<std::uint64_t> b = graph->AddTask(task_inc, &v);
  corekit::api::Result<std::uint64_t> c = graph->AddTask(task_inc, &v);
  if (!a.ok() || !b.ok() || !c.ok()) return false;
  if (!graph->AddDependency(a.value(), c.value()).ok()) return false;
  if (!graph->AddDependency(b.value(), c.value()).ok()) return false;
  if (!graph->Validate().ok()) return false;

  corekit::task::GraphRunOptions options;
  options.fail_fast = true;
  corekit::api::Result<corekit::task::GraphRunStats> run = graph->RunWithExecutor(executor, options);
  corekit_destroy_executor(executor);
  corekit_destroy_task_graph(graph);

  if (!run.ok()) return false;
  if (run.value().total != 3 || run.value().succeeded != 3 || run.value().failed != 0) return false;
  return v.load(std::memory_order_relaxed) == 3;
}

bool TestIpcRoundTripInProcess() {
  corekit::ipc::IChannel* server = corekit_create_ipc_channel();
  corekit::ipc::IChannel* client = corekit_create_ipc_channel();
  if (server == NULL || client == NULL) return false;

  corekit::ipc::ChannelOptions opt;
  opt.name = "ut_ipc_roundtrip";
  opt.capacity = 16;
  opt.message_max_bytes = 128;

  if (!server->OpenServer(opt).ok()) return false;
  if (!client->OpenClient(opt).ok()) return false;

  const char* text = "hello-ipc";
  if (!server->TrySend(text, static_cast<std::uint32_t>(std::strlen(text) + 1)).ok()) {
    return false;
  }

  char buf[128] = {0};
  corekit::api::Result<std::uint32_t> recv = client->TryRecv(buf, sizeof(buf));
  if (!recv.ok()) return false;
  if (std::strcmp(buf, text) != 0) return false;

  server->Close();
  client->Close();
  corekit_destroy_ipc_channel(server);
  corekit_destroy_ipc_channel(client);
  return true;
}

bool TestBasicConcurrentQueue() {
  corekit::concurrent::BasicMutexQueue<int> q(4);
  if (q.Capacity() != 4) return false;
  if (!q.IsEmpty()) return false;
  if (!q.TryPush(1).ok()) return false;
  if (!q.TryPush(2).ok()) return false;
  int peek = 0;
  if (!q.TryPeek(&peek).ok() || peek != 1) return false;
  int batch_values[] = {4, 5, 6};
  std::size_t pushed = 0;
  if (q.TryPushBatch(batch_values, 3, &pushed).code() != corekit::api::StatusCode::kWouldBlock) {
    return false;
  }
  if (pushed != 2) return false;
  if (q.IsEmpty()) return false;
  corekit::api::Result<int> a = q.TryPop();
  corekit::api::Result<int> b = q.TryPop();
  if (!a.ok() || !b.ok()) return false;
  if (a.value() != 1 || b.value() != 2) return false;
  int pop_out[4] = {0};
  std::size_t popped = 0;
  if (!q.TryPopBatch(pop_out, 4, &popped).ok()) return false;
  if (popped != 2 || pop_out[0] != 4 || pop_out[1] != 5) return false;
  corekit::api::Result<int> c = q.TryPop();
  if (c.ok()) return false;
  if (c.status().code() != corekit::api::StatusCode::kWouldBlock) return false;
  if (!q.TryPushMove(3).ok()) return false;
  if (!q.Clear().ok()) return false;
  return q.IsEmpty();
}

bool TestBasicConcurrentMap() {
  corekit::concurrent::BasicConcurrentMap<int, int> m;
  if (!m.Reserve(16).ok()) return false;
  if (!m.InsertIfAbsent(7, 70).ok()) return false;
  if (m.InsertIfAbsent(7, 99).code() != corekit::api::StatusCode::kWouldBlock) return false;
  if (!m.Contains(7)) return false;
  int out = 0;
  if (!m.TryGet(7, &out).ok() || out != 70) return false;
  bool inserted = false;
  if (!m.InsertOrAssign(8, 80, &inserted).ok() || !inserted) return false;
  if (!m.InsertOrAssign(8, 81, &inserted).ok() || inserted) return false;
  std::vector<int> keys;
  if (!m.SnapshotKeys(&keys).ok()) return false;
  if (keys.size() != 2) return false;
  if (!m.Upsert(7, 70).ok()) return false;
  corekit::api::Result<int> got = m.Find(7);
  if (!got.ok() || got.value() != 70) return false;
  if (!m.Upsert(7, 71).ok()) return false;
  got = m.Find(7);
  if (!got.ok() || got.value() != 71) return false;
  if (!m.Erase(7).ok()) return false;
  if (m.Contains(7)) return false;
  if (!m.Clear().ok()) return false;
  got = m.Find(7);
  if (got.ok()) return false;
  return got.status().code() == corekit::api::StatusCode::kNotFound;
}

bool TestBasicConcurrentSet() {
  corekit::concurrent::BasicConcurrentSet<int> s;
  if (!s.Reserve(8).ok()) return false;
  if (!s.Insert(10).ok()) return false;
  if (!s.Insert(20).ok()) return false;
  if (s.Insert(20).code() != corekit::api::StatusCode::kWouldBlock) return false;
  if (!s.Contains(10) || !s.Contains(20)) return false;
  std::vector<int> keys;
  if (!s.Snapshot(&keys).ok()) return false;
  if (keys.size() != 2) return false;
  if (!s.Erase(10).ok()) return false;
  if (s.Contains(10)) return false;
  if (s.Erase(999).code() != corekit::api::StatusCode::kNotFound) return false;
  if (!s.Clear().ok()) return false;
  return s.ApproxSize() == 0;
}

bool TestBasicRingBuffer() {
  corekit::concurrent::BasicRingBuffer<int> rb(3);
  if (rb.Capacity() != 3) return false;
  if (!rb.IsEmpty() || rb.IsFull()) return false;
  if (!rb.TryPush(1).ok()) return false;
  if (!rb.TryPush(2).ok()) return false;
  if (!rb.TryPush(3).ok()) return false;
  if (!rb.IsFull()) return false;
  if (rb.TryPush(4).code() != corekit::api::StatusCode::kWouldBlock) return false;
  int peek = 0;
  if (!rb.TryPeek(&peek).ok() || peek != 1) return false;
  corekit::api::Result<int> a = rb.TryPop();
  corekit::api::Result<int> b = rb.TryPop();
  if (!a.ok() || !b.ok()) return false;
  if (a.value() != 1 || b.value() != 2) return false;
  if (!rb.TryPush(4).ok()) return false;
  corekit::api::Result<int> c = rb.TryPop();
  corekit::api::Result<int> d = rb.TryPop();
  if (!c.ok() || !d.ok()) return false;
  if (c.value() != 3 || d.value() != 4) return false;
  if (rb.TryPop().status().code() != corekit::api::StatusCode::kWouldBlock) return false;
  if (!rb.Clear().ok()) return false;
  return rb.IsEmpty() && rb.Size() == 0;
}

struct DummyPooled {
  int value;
  DummyPooled() : value(0) {}
};

bool TestBasicObjectPool() {
#define CHECK_POOL(cond, msg) \
  do { \
    if (!(cond)) { \
      std::printf("[POOL-FAIL] %s\n", msg); \
      return false; \
    } \
  } while (0)

  corekit::memory::BasicObjectPool<DummyPooled> pool(16);
  CHECK_POOL(pool.Reserve(2).ok(), "reserve");
  CHECK_POOL(pool.Available() >= 2, "available>=2 after reserve");
  CHECK_POOL(pool.TotalAllocated() >= 2, "total>=2 after reserve");

  corekit::api::Result<DummyPooled*> a = pool.Acquire();
  corekit::api::Result<DummyPooled*> b = pool.Acquire();
  CHECK_POOL(a.ok() && b.ok(), "acquire a/b");
  a.value()->value = 123;
  b.value()->value = 456;
  CHECK_POOL(pool.ReleaseObject(a.value()).ok(), "release a");
  CHECK_POOL(pool.ReleaseObject(a.value()).code() == corekit::api::StatusCode::kInvalidArgument,
             "double release a");
  DummyPooled external;
  CHECK_POOL(pool.ReleaseObject(&external).code() == corekit::api::StatusCode::kInvalidArgument,
             "release external");
  CHECK_POOL(pool.ReleaseObject(b.value()).ok(), "release b");
  CHECK_POOL(pool.Trim(1).ok(), "trim1");
  CHECK_POOL(pool.Available() == 1, "available==1 after trim1");
  CHECK_POOL(pool.Clear().ok(), "clear after trim1");
  CHECK_POOL(pool.Available() == 0 && pool.TotalAllocated() == 0, "empty after clear");
  corekit::api::Result<DummyPooled*> c = pool.Acquire();
  CHECK_POOL(c.ok(), "acquire c");
  CHECK_POOL(pool.ReleaseObject(c.value()).ok(), "release c");
  CHECK_POOL(pool.Trim(0).ok(), "trim0");
  CHECK_POOL(pool.Clear().ok(), "clear final");
  CHECK_POOL(pool.Available() == 0 && pool.TotalAllocated() == 0, "empty final");

#undef CHECK_POOL
  return true;
}

bool TestMoodycamelQueue() {
  corekit::concurrent::MoodycamelQueue<int> q(64);
  if (!q.TryPush(11).ok()) return false;
  if (!q.TryPush(22).ok()) return false;
  int batch[] = {33, 44, 55};
  std::size_t pushed = 0;
  if (!q.TryPushBatch(batch, 3, &pushed).ok() || pushed != 3) return false;
  if (q.TryPeek(NULL).code() != corekit::api::StatusCode::kInvalidArgument) return false;
  int peek = 0;
  if (q.TryPeek(&peek).code() != corekit::api::StatusCode::kUnsupported) return false;
  corekit::api::Result<int> a = q.TryPop();
  corekit::api::Result<int> b = q.TryPop();
  if (!a.ok() || !b.ok()) return false;
  if (a.value() != 11 || b.value() != 22) return false;
  int out[4] = {0};
  std::size_t popped = 0;
  if (!q.TryPopBatch(out, 4, &popped).ok()) return false;
  if (popped != 3 || out[0] != 33 || out[1] != 44 || out[2] != 55) return false;
  corekit::api::Result<int> c = q.TryPop();
  if (c.ok()) return false;
  return c.status().code() == corekit::api::StatusCode::kWouldBlock;
}

bool TestGlobalAllocatorConfigAndMacros() {
  const long long tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string ok_cfg = "corekit_mem_ok_" + std::to_string(tick) + ".json";
  const std::string bad_cfg = "corekit_mem_bad_" + std::to_string(tick) + ".json";
  const std::string fallback_cfg = "corekit_mem_fallback_" + std::to_string(tick) + ".json";

  {
    std::ofstream out(ok_cfg.c_str());
    out << "{ \"memory\": { \"backend\": \"system\", \"strict_backend\": true } }\n";
  }
  {
    std::ofstream out(bad_cfg.c_str());
    out << "{ \"memory\": { \"backend\": \"mimalloc\", \"strict_backend\": true } }\n";
  }
  {
    std::ofstream out(fallback_cfg.c_str());
    out << "{ \"memory\": { \"backend\": \"mimalloc\", \"strict_backend\": false } }\n";
  }

  corekit::api::Status st = corekit::memory::GlobalAllocator::ConfigureFromFile(ok_cfg);
  if (!st.ok()) return false;
  if (corekit::memory::GlobalAllocator::CurrentBackend() != corekit::memory::AllocBackend::kSystem) {
    return false;
  }

  void* raw = COREKIT_ALLOC(256);
  if (raw == NULL) return false;
  COREKIT_FREE(raw);

  int* p = COREKIT_NEW(int, 42);
  if (p == NULL) return false;
  if (*p != 42) return false;
  COREKIT_DELETE(p);

  corekit::api::Status bad = corekit::memory::GlobalAllocator::ConfigureFromFile(bad_cfg);
  if (bad.ok()) return false;

  corekit::api::Status fallback = corekit::memory::GlobalAllocator::ConfigureFromFile(fallback_cfg);
  if (!fallback.ok()) return false;
  if (corekit::memory::GlobalAllocator::CurrentBackend() != corekit::memory::AllocBackend::kSystem) {
    return false;
  }

  std::remove(ok_cfg.c_str());
  std::remove(bad_cfg.c_str());
  std::remove(fallback_cfg.c_str());
  return true;
}

int main() {
  struct TestCase {
    const char* name;
    bool (*fn)();
  };

  const TestCase tests[] = {
      {"api_version", TestApiVersion},
      {"factory_lifecycle", TestFactoryLifecycle},
      {"allocator_basic", TestAllocatorBasic},
      {"executor_submit_wait", TestExecutorSubmitAndWait},
      {"executor_parallel_for", TestExecutorParallelFor},
      {"executor_submit_with_key_and_cancel", TestExecutorSubmitWithKeyAndCancel},
      {"executor_submit_ex_serial_key", TestExecutorSubmitExSerialKey},
      {"executor_wait_all_submitted_before", TestExecutorWaitAllSubmittedBefore},
      {"executor_priority_policy", TestExecutorPriorityPolicy},
      {"task_graph_dependency", TestTaskGraphDependency},
      {"task_graph_validate_and_run_with_executor", TestTaskGraphValidateAndRunWithExecutor},
      {"ipc_roundtrip", TestIpcRoundTripInProcess},
      {"basic_queue", TestBasicConcurrentQueue},
      {"basic_map", TestBasicConcurrentMap},
      {"basic_set", TestBasicConcurrentSet},
      {"basic_ring_buffer", TestBasicRingBuffer},
      {"basic_object_pool", TestBasicObjectPool},
      {"moodycamel_queue", TestMoodycamelQueue},
      {"global_allocator_config_and_macros", TestGlobalAllocatorConfigAndMacros},
  };

  int failed = 0;
  for (std::size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    std::printf("[RUN ] %s\n", tests[i].name);
    std::fflush(stdout);
    bool ok = tests[i].fn();
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << tests[i].name << "\n";
    std::fflush(stdout);
    if (!ok) ++failed;
  }

  return failed == 0 ? 0 : 1;
}
