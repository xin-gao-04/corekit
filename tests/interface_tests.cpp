#include "corekit/corekit.hpp"

#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
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
  corekit::api::Status st =
      executor->ParallelFor(1, 101, 8, &AddIndexTask, &sum);
  corekit_destroy_executor(executor);
  if (!st.ok()) return false;
  return sum.load(std::memory_order_relaxed) == 5050;
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
      {"task_graph_dependency", TestTaskGraphDependency},
      {"ipc_roundtrip", TestIpcRoundTripInProcess},
  };

  int failed = 0;
  for (std::size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    bool ok = tests[i].fn();
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << tests[i].name << "\n";
    if (!ok) ++failed;
  }

  return failed == 0 ? 0 : 1;
}


