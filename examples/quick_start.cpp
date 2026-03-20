/// quick_start.cpp — Corekit 快速上手示例
///
/// 展示 corekit 所有核心模块的基本用法，一个文件跑通全流程。
/// 编译：链接 corekit 即可。

#include "corekit/corekit.hpp"

#include <cstdio>
#include <cstring>
#include <string>

// ============================================================================
// 1. UniqueHandle — RAII 生命周期管理（零泄露）
// ============================================================================
static void demo_unique_handle() {
  std::printf("\n=== 1. UniqueHandle (RAII) ===\n");

  // 用 UniqueHandle 包裹工厂创建的对象，离开作用域自动释放。
  corekit::ExecutorHandle exec(corekit_create_executor());
  std::printf("  Executor name: %s\n", exec->Name());

  // 也可以手动 Detach 交出所有权。
  corekit::task::IExecutor* raw = exec.Detach();
  std::printf("  Detached, manual Release needed.\n");
  raw->Release();
}

// ============================================================================
// 2. Executor + Lambda — 现代化任务提交
// ============================================================================
static void demo_executor_lambda() {
  std::printf("\n=== 2. Executor + Lambda ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  int result = 0;

  // 直接提交 lambda。
  exec->Submit([&result]() { result = 42; });
  exec->WaitAll();
  std::printf("  Lambda result: %d\n", result);

  // SubmitEx — 带优先级
  corekit::task::TaskSubmitOptions opts;
  opts.priority = corekit::task::TaskPriority::kHigh;

  corekit::api::Result<corekit::task::TaskId> tid =
      exec->SubmitEx([&result]() { result = 99; }, opts);

  if (tid.ok()) {
    exec->Wait(tid.value(), 0);
    std::printf("  High-priority task result: %d (id=%llu)\n",
                result, (unsigned long long)tid.value());
  }
}

// ============================================================================
// 3. ParallelFor — 数据并行
// ============================================================================
static void demo_parallel_for() {
  std::printf("\n=== 3. ParallelFor ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  static const std::size_t N = 1000;
  int arr[N];
  for (std::size_t i = 0; i < N; ++i) arr[i] = 0;

  // 将 [0, N) 按 grain=100 切分后并行执行。
  exec->ParallelFor(0, N, 100, [&arr](std::size_t index) {
    arr[index] = static_cast<int>(index * 2);
  });

  int sum = 0;
  for (std::size_t i = 0; i < N; ++i) sum += arr[i];
  std::printf("  ParallelFor sum(i*2, i=0..999) = %d (expected %d)\n",
              sum, 999 * 1000);
}

// ============================================================================
// 4. Task Graph — DAG 依赖执行
// ============================================================================
static void demo_task_graph() {
  std::printf("\n=== 4. Task Graph (DAG) ===\n");

  corekit::TaskGraphHandle graph(corekit_create_task_graph());
  corekit::ExecutorHandle exec(corekit_create_executor());

  // 创建 A → B → C 的依赖链
  int order[3] = {0, 0, 0};
  int counter = 0;

  auto add_task = [&](const char*, int idx) -> std::uint64_t {
    corekit::api::Result<corekit::task::TaskId> r = graph->AddTask(
        [&order, &counter, idx]() {
          order[idx] = ++counter;
        });
    return r.value();
  };

  std::uint64_t a = add_task("A", 0);
  std::uint64_t b = add_task("B", 1);
  std::uint64_t c = add_task("C", 2);

  graph->AddDependency(a, b);  // A → B
  graph->AddDependency(b, c);  // B → C

  corekit::api::Status st = graph->Validate();
  std::printf("  Validate: %s\n", st.ok() ? "OK" : st.message().c_str());

  corekit::task::GraphRunOptions run_opts;
  graph->RunWithExecutor(exec.Get(), run_opts);
  std::printf("  Execution order: A=%d, B=%d, C=%d (expected 1,2,3)\n",
              order[0], order[1], order[2]);
}

// ============================================================================
// 5. Memory — 全局分配器 + 宏
// ============================================================================
static void demo_memory() {
  std::printf("\n=== 5. Memory Allocator ===\n");

  std::printf("  Backend: %s\n", corekit::memory::GlobalAllocator::CurrentBackendName());

  // 使用全局分配宏
  int* p = static_cast<int*>(COREKIT_ALLOC(sizeof(int) * 10));
  if (p) {
    for (int i = 0; i < 10; ++i) p[i] = i * i;
    std::printf("  COREKIT_ALLOC: p[5] = %d\n", p[5]);
    COREKIT_FREE(p);
  }

  // 使用 COREKIT_NEW / COREKIT_DELETE
  struct Point { float x, y, z; };
  Point* pt = COREKIT_NEW(Point);
  if (pt) {
    pt->x = 1.0f; pt->y = 2.0f; pt->z = 3.0f;
    std::printf("  COREKIT_NEW Point: (%.1f, %.1f, %.1f)\n", pt->x, pt->y, pt->z);
    COREKIT_DELETE(pt);
  }

  // 查看统计
  corekit::memory::AllocatorStats stats = corekit::memory::GlobalAllocator::CurrentStats();
  std::printf("  Stats: alloc=%llu, free=%llu, in_use=%llu bytes\n",
              (unsigned long long)stats.alloc_count,
              (unsigned long long)stats.free_count,
              (unsigned long long)stats.bytes_in_use);
}

// ============================================================================
// 6. Result 链式 API
// ============================================================================
static void demo_result_chain() {
  std::printf("\n=== 6. Result<T> Chain API ===\n");

  corekit::AllocatorHandle alloc(corekit_create_allocator());

  // ValueOr — 安全取值
  corekit::api::Result<void*> r = alloc->Allocate(64, sizeof(void*));
  void* ptr = r.ValueOr(NULL);
  std::printf("  ValueOr: ptr=%s\n", ptr ? "valid" : "null");
  if (ptr) alloc->Deallocate(ptr);

  // Map — 转换值
  corekit::api::Result<int> num(42);
  corekit::api::Result<int> doubled = num.Map([](const int& v) { return v * 2; });
  std::printf("  Map(42 * 2) = %d\n", doubled.value());

  // AndThen — 链式操作
  corekit::api::Result<int> chained = num.AndThen(
      [](const int& v) -> corekit::api::Result<int> {
        if (v > 0) return corekit::api::Result<int>(v + 100);
        return corekit::api::Result<int>(
            corekit::api::Status(corekit::api::StatusCode::kInvalidArgument, "negative"));
      });
  std::printf("  AndThen(42 + 100) = %d\n", chained.value());
}

// ============================================================================
// 7. IPC Channel (进程内 roundtrip 演示)
// ============================================================================
static void demo_ipc() {
  std::printf("\n=== 7. IPC Channel ===\n");

  corekit::ChannelHandle server(corekit_create_ipc_channel());
  corekit::ChannelHandle client(corekit_create_ipc_channel());

  corekit::ipc::ChannelOptions opts;
  opts.name = "quick_start_demo";
  opts.capacity = 16;
  opts.message_max_bytes = 256;

  corekit::api::Status st = server->OpenServer(opts);
  if (!st.ok()) {
    std::printf("  OpenServer: %s (skip on this platform)\n", st.message().c_str());
    return;
  }
  client->OpenClient(opts);

  // Send from server, recv on client.
  const char* msg = "hello from corekit!";
  server->TrySend(msg, static_cast<std::uint32_t>(std::strlen(msg)));

  char buf[256] = {0};
  corekit::api::Result<std::uint32_t> recv = client->TryRecv(buf, sizeof(buf));
  if (recv.ok()) {
    std::printf("  Received: \"%.*s\"\n", (int)recv.value(), buf);
  }

  corekit::ipc::ChannelStats cs = server->GetStats();
  std::printf("  Stats: send_ok=%llu, recv_ok=%llu\n",
              (unsigned long long)cs.send_ok, (unsigned long long)cs.recv_ok);
}

// ============================================================================
// 8. 日志系统
// ============================================================================
static void demo_logging() {
  std::printf("\n=== 8. Logging ===\n");

  corekit::LogManagerHandle logger(corekit_create_log_manager());

  corekit::api::Status st = logger->Init("quick_start", "");
  if (st.ok()) {
    logger->Log(corekit::log::LogSeverity::kInfo, "Quick start demo running");
    logger->Log(corekit::log::LogSeverity::kWarning, "This is a demo warning");
    std::printf("  Logged 2 messages via ILogManager\n");
    logger->Shutdown();
  } else {
    std::printf("  Logger init: %s\n", st.message().c_str());
  }
}

// ============================================================================
int main() {
  std::printf("====== Corekit Quick Start ======\n");
  std::printf("API version: 0x%08X\n", corekit_get_api_version());

  demo_unique_handle();
  demo_executor_lambda();
  demo_parallel_for();
  demo_task_graph();
  demo_memory();
  demo_result_chain();
  demo_ipc();
  demo_logging();

  std::printf("\n====== All demos completed ======\n");
  return 0;
}
