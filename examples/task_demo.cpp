// 任务调度模块示例
//
// 演示 IExecutor 和 ITaskGraph 的核心用法，重点展示 lambda 接口：
//
//   Part A - IExecutor
//     1. Submit       : 即发任务（lambda，无需 void* 上下文）
//     2. SubmitEx     : 带优先级选项，返回 TaskId
//     3. IsTaskSucceeded : 查询任务是否成功执行
//     4. SubmitWithKey: 同 key 串行化
//     5. ParallelFor  : 并行 for 循环
//     6. Reconfigure  : 运行时调整队列/策略参数
//
//   Part B - ITaskGraph
//     7. AddTask      : 添加任务节点（lambda）
//     8. AddDependency: 建立 DAG 依赖关系
//     9. Run          : 同步单线程运行
//     10. RunWithExecutor : 并行运行，获取执行统计

#include "corekit/corekit.hpp"

#include <atomic>
#include <cstdio>
#include <string>

// ── Part A: IExecutor ─────────────────────────────────────────────────────────

static void DemoExecutor() {
  std::printf("=== IExecutor ===\n");

  // 创建 4 线程执行器
  corekit::task::ExecutorOptions opts;
  opts.worker_count = 4;
  corekit::task::IExecutor* exec = corekit_create_executor_v2(&opts);
  if (exec == NULL) {
    std::fprintf(stderr, "create executor failed\n");
    return;
  }

  // 1. Submit：直接传 lambda，无需定义函数或 void* 结构体
  std::atomic<int> counter(0);
  for (int i = 0; i < 8; ++i) {
    exec->Submit([&counter]() {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
  }
  exec->WaitAll();
  std::printf("  Submit x8 lambda: counter = %d\n", counter.load());

  // 2. SubmitEx：附带选项，获取 TaskId 用于精确等待
  std::string result_msg;
  corekit::task::TaskSubmitOptions high_opts;
  high_opts.priority = corekit::task::TaskPriority::kHigh;

  corekit::api::Result<corekit::task::TaskId> rid = exec->SubmitEx(
      [&result_msg]() {
        result_msg = "high priority task done";
      },
      high_opts);

  if (rid.ok()) {
    exec->Wait(rid.value(), 0 /*无限等待*/);
    std::printf("  SubmitEx: %s\n", result_msg.c_str());

    // 3. IsTaskSucceeded：明确判断任务是否成功完成（不抛异常）
    corekit::api::Result<bool> ok = exec->IsTaskSucceeded(rid.value());
    if (ok.ok()) {
      std::printf("  IsTaskSucceeded: %s\n", ok.value() ? "true" : "false");
    }
  }

  // 4. SubmitWithKey：同一 key 的任务不并发，适合保护共享资源
  const std::uint64_t RESOURCE_KEY = 42;
  std::string log_buf;
  for (int i = 0; i < 3; ++i) {
    exec->SubmitWithKey(RESOURCE_KEY, [&log_buf, i]() {
      // 因为串行化保证，这里操作 log_buf 不需要加锁
      log_buf += "item" + std::to_string(i) + " ";
    });
  }
  exec->WaitAll();
  std::printf("  SubmitWithKey serial log: %s\n", log_buf.c_str());

  // 5. ParallelFor：并行 for 循环，直接传接受 size_t 的 lambda
  std::atomic<long long> sum(0);
  exec->ParallelFor(0, 100, 10, [&sum](std::size_t i) {
    sum.fetch_add(static_cast<long long>(i), std::memory_order_relaxed);
  });
  // sum = 0+1+...+99 = 4950
  std::printf("  ParallelFor [0,100) sum = %lld  (expected 4950)\n", sum.load());

  // 6. Reconfigure：运行时调整队列容量和调度策略（无需重建执行器）
  corekit::task::ExecutorOptions new_opts;
  new_opts.queue_capacity = 512;
  new_opts.policy = corekit::task::ExecutorPolicy::kFifo;
  exec->Reconfigure(new_opts);
  std::printf("  Reconfigure: policy=kFifo, queue_capacity=512\n");

  // 统计信息
  corekit::api::Result<corekit::task::ExecutorStats> stats = exec->QueryStats();
  if (stats.ok()) {
    std::printf("  stats: submitted=%llu completed=%llu failed=%llu\n",
                static_cast<unsigned long long>(stats.value().submitted),
                static_cast<unsigned long long>(stats.value().completed),
                static_cast<unsigned long long>(stats.value().failed));
  }

  corekit_destroy_executor(exec);
}

// ── Part B: ITaskGraph ────────────────────────────────────────────────────────

static void DemoTaskGraph() {
  std::printf("=== ITaskGraph ===\n");

  // 演示如下 DAG（load 和 parse 并行 → process → save）：
  //   load ──┐
  //           ├──▶ process ──▶ save
  //   parse ──┘

  std::atomic<int> step(0);

  corekit::task::ITaskGraph* graph = corekit_create_task_graph();
  if (graph == NULL) {
    std::fprintf(stderr, "create task graph failed\n");
    return;
  }

  // AddTask 直接接受函数指针（内部可包装 lambda 数据）
  // 这里使用静态 lambda 捕获 step 来演示执行顺序
  static std::atomic<int>* g_step = &step;

  auto make_step_fn = [](const char* name) -> void (*)(void*) {
    (void)name;
    return [](void* p) {
      int s = g_step->fetch_add(1, std::memory_order_relaxed);
      std::printf("  [step %d] %s\n", s, static_cast<const char*>(p));
    };
  };

  static const char* kLoad    = "load";
  static const char* kParse   = "parse";
  static const char* kProcess = "process";
  static const char* kSave    = "save";

  corekit::task::GraphTaskOptions name_only;
  name_only.name = "load";
  corekit::api::Result<corekit::task::TaskId> idLoad =
      graph->AddTask(make_step_fn("load"), const_cast<char*>(kLoad), name_only);

  name_only.name = "parse";
  corekit::api::Result<corekit::task::TaskId> idParse =
      graph->AddTask(make_step_fn("parse"), const_cast<char*>(kParse), name_only);

  name_only.name = "process";
  corekit::api::Result<corekit::task::TaskId> idProcess =
      graph->AddTask(make_step_fn("process"), const_cast<char*>(kProcess), name_only);

  name_only.name = "save";
  corekit::api::Result<corekit::task::TaskId> idSave =
      graph->AddTask(make_step_fn("save"), const_cast<char*>(kSave), name_only);

  if (!idLoad.ok() || !idParse.ok() || !idProcess.ok() || !idSave.ok()) {
    std::fprintf(stderr, "AddTask failed\n");
    corekit_destroy_task_graph(graph);
    return;
  }

  graph->AddDependency(idLoad.value(), idProcess.value());
  graph->AddDependency(idParse.value(), idProcess.value());
  graph->AddDependency(idProcess.value(), idSave.value());

  // 校验 DAG
  corekit::api::Status vst = graph->Validate();
  std::printf("  Validate: %s\n", vst.ok() ? "ok" : vst.message().c_str());

  // 9. Run：同步单线程，适合简单串行流水线或测试场景
  std::printf("  -- Run() [sync] --\n");
  step.store(0);
  corekit::api::Result<corekit::task::GraphRunStats> r = graph->Run();
  if (r.ok()) {
    std::printf("  Run: total=%llu succeeded=%llu failed=%llu\n",
                static_cast<unsigned long long>(r.value().total),
                static_cast<unsigned long long>(r.value().succeeded),
                static_cast<unsigned long long>(r.value().failed));
  }

  // 10. RunWithExecutor：利用线程池并行执行同层节点
  std::printf("  -- RunWithExecutor [parallel] --\n");
  step.store(0);
  corekit::task::IExecutor* exec = corekit_create_executor();
  corekit::task::GraphRunOptions run_opts;
  run_opts.fail_fast = true;
  run_opts.max_concurrency = 0;

  corekit::api::Result<corekit::task::GraphRunStats> pr =
      graph->RunWithExecutor(exec, run_opts);
  if (pr.ok()) {
    std::printf("  RunWithExecutor: total=%llu succeeded=%llu failed=%llu\n",
                static_cast<unsigned long long>(pr.value().total),
                static_cast<unsigned long long>(pr.value().succeeded),
                static_cast<unsigned long long>(pr.value().failed));
  }

  corekit_destroy_executor(exec);
  corekit_destroy_task_graph(graph);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
  DemoExecutor();
  DemoTaskGraph();
  return 0;
}
