// 任务调度模块示例
//
// 演示 IExecutor 和 ITaskGraph 的核心用法：
//   Part A - IExecutor
//     1. 创建线程池执行器
//     2. Submit：提交即发任务
//     3. SubmitEx：提交带优先级的任务，获取 TaskId
//     4. SubmitWithKey：同 key 任务串行化
//     5. ParallelFor：并行 for 循环
//     6. WaitAll / 统计
//   Part B - ITaskGraph
//     7. 构建带依赖关系的 DAG
//     8. 校验并运行任务图

#include "corekit/corekit.hpp"

#include <atomic>
#include <cstdio>

// ── Part A: IExecutor ─────────────────────────────────────────────────────

static std::atomic<int> g_counter(0);

static void IncrTask(void* /*unused*/) {
  g_counter.fetch_add(1);
}

static void PrintTask(void* data) {
  std::printf("  task: %s\n", static_cast<const char*>(data));
}

static void DemoExecutor() {
  std::printf("=== IExecutor ===\n");

  // 创建线程池：4 个工作线程，默认混合公平优先级调度策略
  corekit::task::ExecutorOptions opts;
  opts.worker_count = 4;
  corekit::task::IExecutor* exec = corekit_create_executor_v2(&opts);
  if (exec == NULL) {
    std::fprintf(stderr, "create executor failed\n");
    return;
  }

  // 1. Submit：最简接口，提交即发任务（不返回 ID）
  g_counter.store(0);
  for (int i = 0; i < 8; ++i) {
    exec->Submit(IncrTask, NULL);
  }
  exec->WaitAll();
  std::printf("  Submit x8, counter = %d\n", g_counter.load());

  // 2. SubmitEx：提交带调度选项的任务，返回 TaskId 可用于 Wait/TryCancel
  corekit::task::TaskSubmitOptions high_opts;
  high_opts.priority = corekit::task::TaskPriority::kHigh;
  corekit::api::Result<corekit::task::TaskId> rid =
      exec->SubmitEx(PrintTask, const_cast<char*>("high priority task"), high_opts);
  if (rid.ok()) {
    exec->Wait(rid.value(), 0 /*无限等待*/);
  }

  // 3. SubmitWithKey：相同 key 的任务不会并发执行（串行化保证）
  //    适合需要顺序处理特定资源的场景
  const std::uint64_t MY_RESOURCE_KEY = 1001;
  for (int i = 0; i < 4; ++i) {
    exec->SubmitWithKey(MY_RESOURCE_KEY, IncrTask, NULL);
  }
  exec->WaitAll();

  // 4. ParallelFor：并行处理 [0, 16) 区间，每次处理 2 个元素
  g_counter.store(0);
  exec->ParallelFor(0, 16, 2,
                    [](std::size_t /*idx*/, void* /*data*/) {
                      g_counter.fetch_add(1);
                    },
                    NULL);
  std::printf("  ParallelFor [0,16) grain=2, counter = %d\n", g_counter.load());

  // 5. 统计信息
  corekit::api::Result<corekit::task::ExecutorStats> stats = exec->QueryStats();
  if (stats.ok()) {
    std::printf("  stats: submitted=%llu completed=%llu queue_depth=%zu\n",
                static_cast<unsigned long long>(stats.value().submitted),
                static_cast<unsigned long long>(stats.value().completed),
                stats.value().queue_depth);
  }

  corekit_destroy_executor(exec);
}

// ── Part B: ITaskGraph ────────────────────────────────────────────────────

// 记录任务执行顺序
static std::atomic<int> g_step(0);

struct StepData {
  const char* name;
  int expected_step;
};

static void StepTask(void* data) {
  StepData* sd = static_cast<StepData*>(data);
  int step = g_step.fetch_add(1);
  std::printf("  [step %d] %s\n", step, sd->name);
}

static void DemoTaskGraph() {
  std::printf("=== ITaskGraph ===\n");

  // 构建如下依赖图（A, B 并行 → C → D）：
  //   A ──┐
  //        ├──▶ C ──▶ D
  //   B ──┘

  static StepData da = {"task_A", 0};
  static StepData db = {"task_B", 0};
  static StepData dc = {"task_C", 0};
  static StepData dd = {"task_D", 0};

  corekit::task::ITaskGraph* graph = corekit_create_task_graph();
  if (graph == NULL) {
    std::fprintf(stderr, "create task graph failed\n");
    return;
  }

  // 添加节点
  corekit::api::Result<std::uint64_t> idA = graph->AddTask(StepTask, &da);
  corekit::api::Result<std::uint64_t> idB = graph->AddTask(StepTask, &db);
  corekit::api::Result<std::uint64_t> idC = graph->AddTask(StepTask, &dc);
  corekit::api::Result<std::uint64_t> idD = graph->AddTask(StepTask, &dd);

  if (!idA.ok() || !idB.ok() || !idC.ok() || !idD.ok()) {
    std::fprintf(stderr, "AddTask failed\n");
    corekit_destroy_task_graph(graph);
    return;
  }

  // 添加依赖：A→C, B→C, C→D
  graph->AddDependency(idA.value(), idC.value());
  graph->AddDependency(idB.value(), idC.value());
  graph->AddDependency(idC.value(), idD.value());

  // 校验（环检测）
  corekit::api::Status vst = graph->Validate();
  std::printf("  Validate: %s\n", vst.ok() ? "ok" : vst.message().c_str());

  // 使用外部执行器运行，支持 fail_fast 和并发度控制
  corekit::task::IExecutor* exec = corekit_create_executor();
  corekit::task::GraphRunOptions run_opts;
  run_opts.fail_fast = true;
  run_opts.max_concurrency = 0;  // 0 = 不限制
  corekit::api::Result<corekit::task::GraphRunStats> run_stats =
      graph->RunWithExecutor(exec, run_opts);

  if (run_stats.ok()) {
    std::printf("  RunWithExecutor: total=%llu succeeded=%llu failed=%llu\n",
                static_cast<unsigned long long>(run_stats.value().total),
                static_cast<unsigned long long>(run_stats.value().succeeded),
                static_cast<unsigned long long>(run_stats.value().failed));
  }

  corekit_destroy_executor(exec);
  corekit_destroy_task_graph(graph);
}

// ── main ──────────────────────────────────────────────────────────────────

int main() {
  DemoExecutor();
  DemoTaskGraph();
  return 0;
}
