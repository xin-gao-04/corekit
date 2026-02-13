#include "corekit/corekit.hpp"
#include "src/memory/basic_object_pool_impl.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct BenchObj {
  std::uint64_t a;
  std::uint64_t b;
  std::uint64_t c;
  std::uint64_t d;
  BenchObj() : a(0), b(0), c(0), d(0) {}
};

typedef std::chrono::high_resolution_clock Clock;

double SecondsSince(const Clock::time_point& start, const Clock::time_point& end) {
  return std::chrono::duration_cast<std::chrono::duration<double> >(end - start).count();
}

void PrintRow(const char* name, std::size_t iterations, double seconds) {
  const double ops_per_sec = seconds > 0.0 ? (static_cast<double>(iterations) / seconds) : 0.0;
  std::printf("%-30s iter=%zu sec=%.6f ops/s=%.2f\n", name, iterations, seconds, ops_per_sec);
}

const char* BackendEnumName(corekit::memory::AllocBackend b) {
  switch (b) {
    case corekit::memory::AllocBackend::kSystem:
      return "system";
    case corekit::memory::AllocBackend::kMimalloc:
      return "mimalloc";
    case corekit::memory::AllocBackend::kTbbScalable:
      return "tbb";
    default:
      return "unknown";
  }
}

bool BenchNewDelete(std::size_t iterations, double* seconds_out) {
  if (seconds_out == NULL) return false;
  const Clock::time_point begin = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    BenchObj* obj = new BenchObj();
    obj->a = i;
    delete obj;
  }
  const Clock::time_point end = Clock::now();
  *seconds_out = SecondsSince(begin, end);
  return true;
}

bool BenchGlobalAllocatorCurrent(std::size_t iterations, double* seconds_out) {
  if (seconds_out == NULL) return false;
  const std::size_t sz = sizeof(BenchObj);
  const std::size_t align = alignof(BenchObj) < sizeof(void*) ? sizeof(void*) : alignof(BenchObj);

  const Clock::time_point begin = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    corekit::api::Result<void*> r = corekit::memory::GlobalAllocator::Allocate(sz, align);
    if (!r.ok() || r.value() == NULL) return false;
    BenchObj* obj = static_cast<BenchObj*>(r.value());
    obj->a = i;
    if (!corekit::memory::GlobalAllocator::Deallocate(r.value()).ok()) return false;
  }
  const Clock::time_point end = Clock::now();
  *seconds_out = SecondsSince(begin, end);
  return true;
}

bool BenchObjectPool(std::size_t iterations, double* seconds_out) {
  if (seconds_out == NULL) return false;
  corekit::memory::BasicObjectPool<BenchObj> pool(2048);
  if (!pool.Reserve(1024).ok()) return false;

  const Clock::time_point begin = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    corekit::api::Result<BenchObj*> r = pool.Acquire();
    if (!r.ok() || r.value() == NULL) return false;
    r.value()->a = i;
    if (!pool.ReleaseObject(r.value()).ok()) return false;
  }
  const Clock::time_point end = Clock::now();

  if (!pool.Clear().ok()) return false;
  *seconds_out = SecondsSince(begin, end);
  return true;
}

bool TryBenchBackend(corekit::memory::AllocBackend backend,
                     std::size_t iterations,
                     bool* ran,
                     double* seconds_out) {
  if (ran == NULL || seconds_out == NULL) return false;
  *ran = false;
  *seconds_out = 0.0;

  corekit::memory::GlobalAllocatorOptions opt;
  opt.backend = backend;
  opt.strict_backend = true;
  corekit::api::Status st = corekit::memory::GlobalAllocator::Configure(opt);
  if (!st.ok()) {
    if (st.code() == corekit::api::StatusCode::kUnsupported) {
      return true;  // backend not available in this build/environment
    }
    return false;
  }

  if (!BenchGlobalAllocatorCurrent(iterations, seconds_out)) return false;
  *ran = true;

  corekit::memory::GlobalAllocatorOptions reset;
  reset.backend = corekit::memory::AllocBackend::kSystem;
  reset.strict_backend = true;
  return corekit::memory::GlobalAllocator::Configure(reset).ok();
}

}  // namespace

int main(int argc, char** argv) {
  std::size_t iterations = 300000;
  if (argc > 1) {
    const long long n = std::atoll(argv[1]);
    if (n > 0) iterations = static_cast<std::size_t>(n);
  }

  std::printf("[memory-perf] iterations=%zu\n", iterations);

  double t_new_delete = 0.0;
  double t_pool = 0.0;
  if (!BenchNewDelete(iterations, &t_new_delete)) {
    std::printf("new/delete bench failed\n");
    return 1;
  }
  if (!BenchObjectPool(iterations, &t_pool)) {
    std::printf("object pool bench failed\n");
    return 1;
  }

  PrintRow("new_delete", iterations, t_new_delete);
  PrintRow("object_pool", iterations, t_pool);

  const corekit::memory::AllocBackend backends[] = {
      corekit::memory::AllocBackend::kSystem,
      corekit::memory::AllocBackend::kMimalloc,
      corekit::memory::AllocBackend::kTbbScalable,
  };

  for (std::size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); ++i) {
    bool ran = false;
    double sec = 0.0;
    if (!TryBenchBackend(backends[i], iterations, &ran, &sec)) {
      std::printf("backend bench failed: %s\n", BackendEnumName(backends[i]));
      return 1;
    }
    char label[64] = {0};
    std::snprintf(label, sizeof(label), "global_allocator[%s]", BackendEnumName(backends[i]));
    if (ran) {
      PrintRow(label, iterations, sec);
    } else {
      std::printf("%-30s SKIP (backend unavailable)\n", label);
    }
  }

  return 0;
}
