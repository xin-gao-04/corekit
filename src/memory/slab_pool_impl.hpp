#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include "corekit/memory/i_global_allocator.hpp"
#include "corekit/memory/i_memory_pool.hpp"

namespace corekit {
namespace memory {

// ============================================================================
// Slab size classes: 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
// Allocations > 4096 go directly to the underlying allocator (large alloc).
// ============================================================================

namespace slab_detail {

static const std::size_t kNumSizeClasses = 10;
static const std::size_t kSizeClasses[kNumSizeClasses] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static const std::size_t kMaxSlabSize = 4096;

// Guard byte pattern for overflow detection.
static const std::uint8_t kGuardPattern = 0xFD;
static const std::size_t kGuardSize = 8;

inline std::size_t SizeClassIndex(std::size_t size) {
  // Find the smallest size class >= size.
  for (std::size_t i = 0; i < kNumSizeClasses; ++i) {
    if (size <= kSizeClasses[i]) return i;
  }
  return kNumSizeClasses;  // Large allocation.
}

// Each slab has a header immediately before the user pointer.
struct SlabHeader {
  std::uint32_t magic;       // 0xC0EE5LAB for validation.
  std::uint16_t class_index; // Size class index (0..9), or 0xFFFF for large.
  std::uint16_t flags;       // Bit 0: has guard bytes.
  std::size_t user_size;     // Actual requested size.
  void* block_owner;         // Owning block (for slab) or NULL (for large).
};

static const std::uint32_t kSlabMagic = 0xC0EE0AAB;

inline SlabHeader* HeaderFromUser(void* ptr) {
  return reinterpret_cast<SlabHeader*>(static_cast<std::uint8_t*>(ptr) -
                                       sizeof(SlabHeader));
}

inline void* UserFromHeader(SlabHeader* hdr) {
  return static_cast<void*>(reinterpret_cast<std::uint8_t*>(hdr) +
                            sizeof(SlabHeader));
}

// A block is a large contiguous allocation subdivided into equal-size slabs.
struct Block {
  std::uint8_t* base;       // Base address of the block allocation.
  std::size_t total_bytes;  // Total block size.
  std::size_t slab_size;    // Size of each slab (including header).
  std::size_t slab_count;   // Number of slabs in this block.
  std::size_t used_count;   // Number of slabs currently in use.
};

}  // namespace slab_detail

class SlabPoolImpl : public IMemoryPool {
 public:
  SlabPoolImpl() : inited_(false), parent_(NULL) {}

  explicit SlabPoolImpl(SlabPoolImpl* parent) : inited_(false), parent_(parent) {}

  virtual ~SlabPoolImpl() {
    // Free all large allocations.
    for (std::size_t i = 0; i < large_allocs_.size(); ++i) {
      GlobalFreeIgnore(large_allocs_[i]);
    }
    // Free all blocks.
    for (std::size_t c = 0; c < slab_detail::kNumSizeClasses; ++c) {
      for (std::size_t b = 0; b < blocks_[c].size(); ++b) {
        GlobalFreeIgnore(blocks_[c][b].base);
      }
    }
    // Release children.
    for (std::size_t i = 0; i < children_.size(); ++i) {
      delete children_[i];
    }
  }

  const char* Name() const override { return config_.name; }
  void Release() override { delete this; }

  api::Status Init(const PoolConfig& config) override {
    if (inited_) {
      return api::Status(api::StatusCode::kAlreadyInitialized, "pool already initialized");
    }
    config_ = config;
    inited_ = true;

    // Pre-populate free lists.
    for (std::size_t i = 0; i < slab_detail::kNumSizeClasses; ++i) {
      // Empty initially; blocks are allocated on first demand.
    }
    return api::Status::Ok();
  }

  void* Alloc(std::size_t size) override {
    return AllocAligned(size, sizeof(void*));
  }

  void* AllocAligned(std::size_t size, std::size_t alignment) override {
    if (size == 0) return NULL;
    if (!inited_) return NULL;

    // Check budget.
    if (config_.max_bytes > 0) {
      std::uint64_t current = stats_.bytes_in_use.load(std::memory_order_relaxed);
      if (current + size > config_.max_bytes) {
        return NULL;  // Budget exceeded.
      }
    }

    const std::size_t class_idx = slab_detail::SizeClassIndex(size);

    void* result = NULL;
    if (class_idx < slab_detail::kNumSizeClasses) {
      result = AllocFromSlab(class_idx, size);
    } else {
      result = AllocLarge(size, alignment);
    }

    if (result != NULL) {
      stats_.alloc_count.fetch_add(1, std::memory_order_relaxed);
      const std::uint64_t in_use =
          stats_.bytes_in_use.fetch_add(size, std::memory_order_relaxed) + size;
      // Update peak.
      std::uint64_t peak = stats_.bytes_peak.load(std::memory_order_relaxed);
      while (in_use > peak &&
             !stats_.bytes_peak.compare_exchange_weak(peak, in_use,
                                                      std::memory_order_relaxed)) {
      }
      // Also track in parent.
      if (parent_ != NULL) {
        parent_->stats_.bytes_in_use.fetch_add(size, std::memory_order_relaxed);
      }
    }
    return result;
  }

  void Free(void* ptr) override {
    if (ptr == NULL || !inited_) return;

    slab_detail::SlabHeader* hdr = slab_detail::HeaderFromUser(ptr);
    if (hdr->magic != slab_detail::kSlabMagic) {
      return;  // Not our allocation (or corruption).
    }

    // Validate guard bytes if enabled.
    if (hdr->flags & 0x01) {
      ValidateGuard(ptr, hdr->user_size);
    }

    const std::size_t freed_size = hdr->user_size;

    if (hdr->class_index == 0xFFFF) {
      FreeLarge(hdr);
    } else {
      FreeToSlab(hdr);
    }

    stats_.free_count.fetch_add(1, std::memory_order_relaxed);
    std::uint64_t cur = stats_.bytes_in_use.load(std::memory_order_relaxed);
    while (true) {
      std::uint64_t next = cur > freed_size ? (cur - freed_size) : 0;
      if (stats_.bytes_in_use.compare_exchange_weak(cur, next,
                                                     std::memory_order_relaxed)) {
        break;
      }
    }
    if (parent_ != NULL) {
      std::uint64_t pcur = parent_->stats_.bytes_in_use.load(std::memory_order_relaxed);
      while (true) {
        std::uint64_t pnext = pcur > freed_size ? (pcur - freed_size) : 0;
        if (parent_->stats_.bytes_in_use.compare_exchange_weak(pcur, pnext,
                                                                std::memory_order_relaxed)) {
          break;
        }
      }
    }
  }

  api::Result<IMemoryPool*> CreateChild(const PoolConfig& config) override {
    if (!inited_) {
      return api::Result<IMemoryPool*>(
          api::Status(api::StatusCode::kNotInitialized, "parent pool not initialized"));
    }
    SlabPoolImpl* child = new (std::nothrow) SlabPoolImpl(this);
    if (child == NULL) {
      return api::Result<IMemoryPool*>(
          api::Status(api::StatusCode::kInternalError, "failed to allocate child pool"));
    }
    api::Status st = child->Init(config);
    if (!st.ok()) {
      delete child;
      return api::Result<IMemoryPool*>(st);
    }

    if (config_.thread_safe) {
      std::lock_guard<std::mutex> lock(mu_);
      children_.push_back(child);
    } else {
      children_.push_back(child);
    }
    return api::Result<IMemoryPool*>(static_cast<IMemoryPool*>(child));
  }

  PoolStats Stats() const override {
    PoolStats out;
    out.alloc_count = stats_.alloc_count.load(std::memory_order_relaxed);
    out.free_count = stats_.free_count.load(std::memory_order_relaxed);
    out.bytes_in_use = stats_.bytes_in_use.load(std::memory_order_relaxed);
    out.bytes_peak = stats_.bytes_peak.load(std::memory_order_relaxed);
    out.bytes_limit = config_.max_bytes;
    out.slab_hits = stats_.slab_hits.load(std::memory_order_relaxed);
    out.slab_misses = stats_.slab_misses.load(std::memory_order_relaxed);

    // Count blocks.
    std::uint64_t bc = 0;
    for (std::size_t c = 0; c < slab_detail::kNumSizeClasses; ++c) {
      bc += blocks_[c].size();
    }
    out.block_count = bc;
    return out;
  }

  api::Status Shrink() override {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t c = 0; c < slab_detail::kNumSizeClasses; ++c) {
      // Remove blocks where all slabs are free.
      std::deque<slab_detail::Block> kept;
      for (std::size_t b = 0; b < blocks_[c].size(); ++b) {
        if (blocks_[c][b].used_count == 0) {
          GlobalFreeIgnore(blocks_[c][b].base);
        } else {
          kept.push_back(blocks_[c][b]);
        }
      }
      blocks_[c].swap(kept);

      // Rebuild free list from remaining blocks.
      free_lists_[c].clear();
      for (std::size_t b = 0; b < blocks_[c].size(); ++b) {
        RebuildFreeList(c, blocks_[c][b]);
      }
    }
    return api::Status::Ok();
  }

  bool Owns(const void* ptr) const override {
    if (ptr == NULL) return false;
    const slab_detail::SlabHeader* hdr = reinterpret_cast<const slab_detail::SlabHeader*>(
        static_cast<const std::uint8_t*>(ptr) - sizeof(slab_detail::SlabHeader));
    return hdr->magic == slab_detail::kSlabMagic;
  }

 private:
  struct AtomicStats {
    std::atomic<std::uint64_t> alloc_count{0};
    std::atomic<std::uint64_t> free_count{0};
    std::atomic<std::uint64_t> bytes_in_use{0};
    std::atomic<std::uint64_t> bytes_peak{0};
    std::atomic<std::uint64_t> slab_hits{0};
    std::atomic<std::uint64_t> slab_misses{0};
  };

  PoolConfig config_;
  bool inited_;
  SlabPoolImpl* parent_;
  AtomicStats stats_;
  std::mutex mu_;

  // Per-size-class: block list (deque for pointer stability) + free list.
  std::deque<slab_detail::Block> blocks_[slab_detail::kNumSizeClasses];
  std::vector<void*> free_lists_[slab_detail::kNumSizeClasses];

  // Large (>4096) allocations tracked for cleanup.
  std::vector<void*> large_allocs_;

  // Child pools.
  std::vector<SlabPoolImpl*> children_;

  void* AllocFromSlab(std::size_t class_idx, std::size_t user_size) {
    std::lock_guard<std::mutex> lock(mu_);

    // Try free list first.
    if (!free_lists_[class_idx].empty()) {
      void* slot = free_lists_[class_idx].back();
      free_lists_[class_idx].pop_back();

      slab_detail::SlabHeader* hdr = static_cast<slab_detail::SlabHeader*>(slot);
      hdr->magic = slab_detail::kSlabMagic;
      hdr->class_index = static_cast<std::uint16_t>(class_idx);
      hdr->flags = config_.enable_guard_bytes ? 0x01 : 0x00;
      hdr->user_size = user_size;

      void* user_ptr = slab_detail::UserFromHeader(hdr);

      if (config_.enable_guard_bytes) {
        WriteGuard(user_ptr, user_size, slab_detail::kSizeClasses[class_idx]);
      }

      // Track block usage.
      if (hdr->block_owner) {
        slab_detail::Block* blk = static_cast<slab_detail::Block*>(hdr->block_owner);
        blk->used_count++;
      }

      stats_.slab_hits.fetch_add(1, std::memory_order_relaxed);
      return user_ptr;
    }

    // Allocate a new block.
    const std::size_t slab_size = sizeof(slab_detail::SlabHeader) +
                                  slab_detail::kSizeClasses[class_idx] +
                                  (config_.enable_guard_bytes ? slab_detail::kGuardSize : 0);
    const std::size_t block_bytes = config_.block_size;
    const std::size_t slab_count = block_bytes / slab_size;
    if (slab_count == 0) return NULL;

    void* raw = GlobalAllocOrNull(block_bytes, sizeof(void*));
    if (raw == NULL) return NULL;
    std::memset(raw, 0, block_bytes);

    slab_detail::Block blk;
    blk.base = static_cast<std::uint8_t*>(raw);
    blk.total_bytes = block_bytes;
    blk.slab_size = slab_size;
    blk.slab_count = slab_count;
    blk.used_count = 1;  // We'll hand out the first one immediately.
    blocks_[class_idx].push_back(blk);

    slab_detail::Block* blk_ptr = &blocks_[class_idx].back();

    // Populate free list with slabs [1..N-1].
    for (std::size_t i = 1; i < slab_count; ++i) {
      std::uint8_t* slot = blk.base + i * slab_size;
      slab_detail::SlabHeader* fhdr = reinterpret_cast<slab_detail::SlabHeader*>(slot);
      fhdr->block_owner = blk_ptr;
      free_lists_[class_idx].push_back(slot);
    }

    // Return the first slab.
    slab_detail::SlabHeader* hdr = reinterpret_cast<slab_detail::SlabHeader*>(blk.base);
    hdr->magic = slab_detail::kSlabMagic;
    hdr->class_index = static_cast<std::uint16_t>(class_idx);
    hdr->flags = config_.enable_guard_bytes ? 0x01 : 0x00;
    hdr->user_size = user_size;
    hdr->block_owner = blk_ptr;

    void* user_ptr = slab_detail::UserFromHeader(hdr);
    if (config_.enable_guard_bytes) {
      WriteGuard(user_ptr, user_size, slab_detail::kSizeClasses[class_idx]);
    }

    stats_.slab_misses.fetch_add(1, std::memory_order_relaxed);
    return user_ptr;
  }

  void FreeToSlab(slab_detail::SlabHeader* hdr) {
    std::lock_guard<std::mutex> lock(mu_);
    const std::size_t class_idx = hdr->class_index;
    if (class_idx >= slab_detail::kNumSizeClasses) return;

    if (hdr->block_owner) {
      slab_detail::Block* blk = static_cast<slab_detail::Block*>(hdr->block_owner);
      if (blk->used_count > 0) blk->used_count--;
    }

    hdr->magic = 0;  // Invalidate.
    free_lists_[class_idx].push_back(static_cast<void*>(hdr));
  }

  void* AllocLarge(std::size_t size, std::size_t alignment) {
    const std::size_t total = sizeof(slab_detail::SlabHeader) + size +
                              (config_.enable_guard_bytes ? slab_detail::kGuardSize : 0);
    const std::size_t align = alignment < sizeof(void*) ? sizeof(void*) : alignment;

    void* raw = GlobalAllocOrNull(total, align);
    if (raw == NULL) return NULL;

    slab_detail::SlabHeader* hdr = static_cast<slab_detail::SlabHeader*>(raw);
    hdr->magic = slab_detail::kSlabMagic;
    hdr->class_index = 0xFFFF;
    hdr->flags = config_.enable_guard_bytes ? 0x01 : 0x00;
    hdr->user_size = size;
    hdr->block_owner = NULL;

    void* user_ptr = slab_detail::UserFromHeader(hdr);
    if (config_.enable_guard_bytes) {
      WriteGuard(user_ptr, size, size);
    }

    std::lock_guard<std::mutex> lock(mu_);
    large_allocs_.push_back(raw);
    return user_ptr;
  }

  void FreeLarge(slab_detail::SlabHeader* hdr) {
    void* raw = static_cast<void*>(hdr);
    hdr->magic = 0;

    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < large_allocs_.size(); ++i) {
      if (large_allocs_[i] == raw) {
        large_allocs_[i] = large_allocs_.back();
        large_allocs_.pop_back();
        break;
      }
    }
    GlobalFreeIgnore(raw);
  }

  void WriteGuard(void* user_ptr, std::size_t user_size, std::size_t slab_data_size) {
    // Write guard bytes after user data.
    std::uint8_t* guard_start = static_cast<std::uint8_t*>(user_ptr) + user_size;
    std::size_t guard_len = slab_data_size - user_size;
    if (guard_len > slab_detail::kGuardSize) guard_len = slab_detail::kGuardSize;
    std::memset(guard_start, slab_detail::kGuardPattern, guard_len);
  }

  bool ValidateGuard(void* user_ptr, std::size_t user_size) {
    const std::uint8_t* guard_start =
        static_cast<const std::uint8_t*>(user_ptr) + user_size;
    for (std::size_t i = 0; i < slab_detail::kGuardSize; ++i) {
      if (guard_start[i] != slab_detail::kGuardPattern) {
        return false;  // Buffer overflow detected!
      }
    }
    return true;
  }

  void RebuildFreeList(std::size_t class_idx, slab_detail::Block& blk) {
    for (std::size_t i = 0; i < blk.slab_count; ++i) {
      std::uint8_t* slot = blk.base + i * blk.slab_size;
      slab_detail::SlabHeader* hdr = reinterpret_cast<slab_detail::SlabHeader*>(slot);
      if (hdr->magic != slab_detail::kSlabMagic) {
        // Not in use — add to free list.
        hdr->block_owner = &blk;
        free_lists_[class_idx].push_back(slot);
      }
    }
  }
};

}  // namespace memory
}  // namespace corekit
