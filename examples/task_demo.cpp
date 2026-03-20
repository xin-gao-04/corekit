/// task_demo.cpp — 任务调度系统使用示例
///
/// 展示 corekit 执行器 (Executor) 和任务图 (TaskGraph) 的用法：
///   - Lambda 提交 vs C 风格提交
///   - 串行 key（同 key 任务不并发）
///   - 优先级调度
///   - ParallelFor 数据并行
///   - DAG 任务图
///   - 执行器统计

#include "corekit/corekit.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// 1. Lambda 提交 — 最简用法
// ============================================================================
static void demo_lambda_submit() {
  std::printf("\n=== 1. Lambda Submit ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  std::atomic<int> count(0);

  // 提交 10 个 lambda 任务。
  for (int i = 0; i < 10; ++i) {
    corekit::task::SubmitLambda(exec.Get(), [&count]() {
      count.fetch_add(1, std::memory_order_relaxed);
    });
  }
  exec->WaitAll();
  std::printf("  10 lambdas completed, count = %d\n", count.load());
}

// ============================================================================
// 2. C 风格回调 — 传统用法
// ============================================================================
static void demo_c_style() {
  std::printf("\n=== 2. C-Style Callback ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  struct Context {
    int input;
    int output;
  };
  Context ctx = {7, 0};

  exec->Submit(
      [](void* p) {
        Context* c = static_cast<Context*>(p);
        c->output = c->input * c->input;
      },
      &ctx);
  exec->WaitAll();
  std::printf("  Input=%d, Output=%d (expected %d)\n", ctx.input, ctx.output, 49);
}

// ============================================================================
// 3. 串行 Key — 保证同 key 不并发
// ============================================================================
static void demo_serial_key() {
  std::printf("\n=== 3. Serial Key ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  // key=1 的任务保证串行执行，不会并发。
  // 适合对同一资源（如同一文件、同一 DB 行）的操作。
  std::vector<int> order;
  std::mutex mu;

  for (int i = 0; i < 5; ++i) {
    struct Ctx {
      std::vector<int>* order;
      std::mutex* mu;
      int value;
    };
    Ctx* c = new Ctx{&order, &mu, i};

    corekit::task::SubmitLambdaWithKey(exec.Get(), /*serial_key=*/1, [c]() {
      std::lock_guard<std::mutex> lock(*c->mu);
      c->order->push_back(c->value);
      delete c;
    });
  }
  exec->WaitAll();

  std::printf("  Serial-key order: ");
  for (std::size_t i = 0; i < order.size(); ++i) {
    std::printf("%d ", order[i]);
  }
  std::printf("(should be 0 1 2 3 4)\n");
}

// ============================================================================
// 4. 优先级 — High 先于 Normal 先于 Low
// ============================================================================
static void demo_priority() {
  std::printf("\n=== 4. Priority Scheduling ===\n");

  // 创建单线程执行器，让优先级效果更明显。
  corekit::task::ExecutorOptions opts;
  opts.worker_count = 1;
  opts.policy = corekit::task::ExecutorPolicy::kPriority;
  corekit::ExecutorHandle exec(corekit_create_executor_v2(&opts));

  std::vector<std::string> results;
  std::mutex mu;

  // 先提交 low，再提交 high — high 应先执行。
  auto submit = [&](const char* label, corekit::task::TaskPriority pri) {
    corekit::task::TaskSubmitOptions topts;
    topts.priority = pri;
    corekit::task::SubmitLambdaEx(exec.Get(), [&results, &mu, label]() {
      std::lock_guard<std::mutex> lock(mu);
      results.push_back(label);
    }, topts);
  };

  submit("low", corekit::task::TaskPriority::kLow);
  submit("normal", corekit::task::TaskPriority::kNormal);
  submit("high", corekit::task::TaskPriority::kHigh);

  exec->WaitAll();

  std::printf("  Execution order: ");
  for (std::size_t i = 0; i < results.size(); ++i) {
    std::printf("%s ", results[i].c_str());
  }
  std::printf("\n");
}

// ============================================================================
// 5. ParallelFor — 图像处理模拟
// ============================================================================
static void demo_parallel_image() {
  std::printf("\n=== 5. ParallelFor (Image Processing) ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  // 模拟一张 1000x1000 灰度图的反色操作。
  static const std::size_t W = 1000, H = 1000;
  std::vector<unsigned char> image(W * H, 200);

  exec->ParallelFor(0, H, 50,
      [](std::size_t row, void* ctx) {
        unsigned char* img = static_cast<unsigned char*>(ctx);
        for (std::size_t col = 0; col < W; ++col) {
          img[row * W + col] = 255 - img[row * W + col];
        }
      },
      image.data());

  std::printf("  Inverted 1000x1000 image\n");
  std::printf("  Pixel[0] = %d (expected 55)\n", (int)image[0]);
  std::printf("  Pixel[999999] = %d (expected 55)\n", (int)image[999999]);
}

// ============================================================================
// 6. Task Graph — 构建管线
// ============================================================================
struct PipeCtx {
  std::vector<std::string>* log;
  std::mutex* mu;
  std::string name;
};

static void demo_pipeline_graph() {
  std::printf("\n=== 6. Task Graph (Build Pipeline) ===\n");

  corekit::TaskGraphHandle graph(corekit_create_task_graph());
  corekit::ExecutorHandle exec(corekit_create_executor());

  // 模拟一个构建管线:
  //   [compile_a] ──┐
  //                 ├──> [link] ──> [package]
  //   [compile_b] ──┘
  //
  //   [gen_docs] ─────────────────> [package]

  std::vector<std::string> log;
  std::mutex mu;

  auto make_task = [&](const char* name) -> std::uint64_t {
    PipeCtx* c = new PipeCtx();
    c->log = &log;
    c->mu = &mu;
    c->name = name;
    corekit::api::Result<std::uint64_t> r = graph->AddTask(
        [](void* p) {
          PipeCtx* ctx = static_cast<PipeCtx*>(p);
          {
            std::lock_guard<std::mutex> lk(*ctx->mu);
            ctx->log->push_back(ctx->name);
          }
          delete ctx;
        }, c);
    return r.value();
  };

  std::uint64_t compile_a = make_task("compile_a");
  std::uint64_t compile_b = make_task("compile_b");
  std::uint64_t gen_docs  = make_task("gen_docs");
  std::uint64_t link      = make_task("link");
  std::uint64_t package   = make_task("package");

  graph->AddDependency(compile_a, link);
  graph->AddDependency(compile_b, link);
  graph->AddDependency(link, package);
  graph->AddDependency(gen_docs, package);

  corekit::api::Status st = graph->Validate();
  std::printf("  DAG validate: %s\n", st.ok() ? "OK" : st.message().c_str());

  corekit::task::GraphRunOptions run_opts;
  corekit::api::Result<corekit::task::GraphRunStats> result =
      graph->RunWithExecutor(exec.Get(), run_opts);

  if (result.ok()) {
    std::printf("  Completed: %llu/%llu tasks\n",
                (unsigned long long)result.value().succeeded,
                (unsigned long long)result.value().total);
  }

  std::printf("  Execution log: ");
  for (std::size_t i = 0; i < log.size(); ++i) {
    std::printf("%s ", log[i].c_str());
  }
  std::printf("\n");

  // Verify: package must be last.
  if (!log.empty()) {
    std::printf("  Last task: %s (must be 'package')\n", log.back().c_str());
  }
}

// ============================================================================
// 7. 执行器统计
// ============================================================================
static void demo_stats() {
  std::printf("\n=== 7. Executor Stats ===\n");

  corekit::ExecutorHandle exec(corekit_create_executor());

  for (int i = 0; i < 100; ++i) {
    corekit::task::SubmitLambda(exec.Get(), []() {
      volatile int x = 0;
      for (int j = 0; j < 1000; ++j) x += j;
      (void)x;
    });
  }
  exec->WaitAll();

  corekit::api::Result<corekit::task::ExecutorStats> r = exec->QueryStats();
  if (r.ok()) {
    const corekit::task::ExecutorStats& s = r.value();
    std::printf("  submitted=%llu, completed=%llu, failed=%llu\n",
                (unsigned long long)s.submitted,
                (unsigned long long)s.completed,
                (unsigned long long)s.failed);
    std::printf("  queue_high_watermark=%zu\n", s.queue_high_watermark);
  }
}

// ============================================================================
int main() {
  std::printf("====== Corekit Task Demo ======\n");

  demo_lambda_submit();
  demo_c_style();
  demo_serial_key();
  demo_priority();
  demo_parallel_image();
  demo_pipeline_graph();
  demo_stats();

  std::printf("\n====== All task demos completed ======\n");
  return 0;
}
