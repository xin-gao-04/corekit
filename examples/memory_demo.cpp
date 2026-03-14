// 内存分配模块示例
//
// 演示 GlobalAllocator（全局分配器门面）和便利宏的用法：
//   1. 配置分配后端（system / tbb / mimalloc）
//   2. 使用宏分配/释放原始内存
//   3. 使用宏创建/销毁 C++ 对象（GlobalNew / GlobalDelete）
//   4. 查询运行时统计信息

#include "corekit/memory/i_global_allocator.hpp"

#include <cstdio>
#include <cstring>

// 用于演示 GlobalNew/GlobalDelete 的简单结构体
struct Packet {
  int id;
  char data[64];

  explicit Packet(int id_) : id(id_) {
    std::snprintf(data, sizeof(data), "payload-%d", id_);
  }
};

int main() {
  // 1. 显式配置分配后端
  //    默认后端为 kSystem（标准 malloc/free），无需额外依赖。
  //    如需 tbb 或 mimalloc，需在 CMake 中开启对应选项并链接库。
  corekit::memory::GlobalAllocatorOptions opts;
  opts.backend = corekit::memory::AllocBackend::kSystem;
  opts.strict_backend = false;  // false：若所选后端不可用，自动回退到 system
  corekit::api::Status st = corekit::memory::GlobalAllocator::Configure(opts);
  if (!st.ok()) {
    std::fprintf(stderr, "Configure failed: %s\n", st.message().c_str());
    return 1;
  }
  std::printf("backend: %s\n", corekit::memory::GlobalAllocator::CurrentBackendName());

  // 2. 原始内存分配（宏版本）
  //    COREKIT_ALLOC(bytes)         : 默认对齐
  //    COREKIT_ALLOC_ALIGNED(b, a)  : 指定对齐
  //    COREKIT_FREE(ptr)            : 释放（nullptr 安全）
  void* raw = COREKIT_ALLOC(128);
  if (raw != NULL) {
    std::memset(raw, 0, 128);
    std::printf("raw alloc: ptr=%p\n", raw);
    COREKIT_FREE(raw);
  }

  // 对齐分配（例如 SIMD 需要 64 字节对齐）
  void* aligned = COREKIT_ALLOC_ALIGNED(256, 64);
  if (aligned != NULL) {
    std::printf("aligned alloc: ptr=%p (alignment check: %s)\n", aligned,
                (reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0) ? "ok" : "fail");
    COREKIT_FREE(aligned);
  }

  // 3. 对象创建/销毁（宏版本）
  //    COREKIT_NEW(Type, args...)  : placement new（失败返回 nullptr）
  //    COREKIT_DELETE(ptr)        : 调用析构函数后释放内存
  Packet* pkt = COREKIT_NEW(Packet, 42);
  if (pkt != NULL) {
    std::printf("packet: id=%d data=%s\n", pkt->id, pkt->data);
    COREKIT_DELETE(pkt);
  }

  // 4. 统计信息
  corekit::memory::AllocatorStats stats = corekit::memory::GlobalAllocator::CurrentStats();
  std::printf("stats: alloc_count=%llu free_count=%llu bytes_peak=%llu\n",
              static_cast<unsigned long long>(stats.alloc_count),
              static_cast<unsigned long long>(stats.free_count),
              static_cast<unsigned long long>(stats.bytes_peak));

  return 0;
}
