// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_

// DESCRIPTION
// PartitionRoot::Alloc() and PartitionRoot::Free() are approximately analogous
// to malloc() and free().
//
// The main difference is that a PartitionRoot object must be supplied to these
// functions, representing a specific "heap partition" that will be used to
// satisfy the allocation. Different partitions are guaranteed to exist in
// separate address spaces, including being separate from the main system
// heap. If the contained objects are all freed, physical memory is returned to
// the system but the address space remains reserved.  See PartitionAlloc.md for
// other security properties PartitionAlloc provides.
//
// THE ONLY LEGITIMATE WAY TO OBTAIN A PartitionRoot IS THROUGH THE
// PartitionAllocator classes. To minimize the instruction count to the fullest
// extent possible, the PartitionRoot is really just a header adjacent to other
// data areas provided by the allocator class.
//
// The constraints for PartitionRoot::Alloc() are:
// - Multi-threaded use against a single partition is ok; locking is handled.
// - Allocations of any arbitrary size can be handled (subject to a limit of
//   INT_MAX bytes for security reasons).
// - Bucketing is by approximate size, for example an allocation of 4000 bytes
//   might be placed into a 4096-byte bucket. Bucket sizes are chosen to try and
//   keep worst-case waste to ~10%.

#include <atomic>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bits.h"
#include "base/optional.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"

// We use this to make MEMORY_TOOL_REPLACES_ALLOCATOR behave the same for max
// size as other alloc code.
#define CHECK_MAX_SIZE_OR_RETURN_NULLPTR(size, flags) \
  if (size > MaxDirectMapped()) {                     \
    if (flags & PartitionAllocReturnNull) {           \
      return nullptr;                                 \
    }                                                 \
    PA_CHECK(false);                                  \
  }

namespace base {

namespace internal {
// Avoid including partition_address_space.h from this .h file, by moving the
// call to IsManagedByPartitionAllocBRPPool into the .cc file.
#if DCHECK_IS_ON()
BASE_EXPORT void DCheckIfManagedByPartitionAllocBRPPool(void* ptr);
#else
ALWAYS_INLINE void DCheckIfManagedByPartitionAllocBRPPool(void*) {}
#endif
}  // namespace internal

enum PartitionPurgeFlags {
  // Decommitting the ring list of empty slot spans is reasonably fast.
  PartitionPurgeDecommitEmptySlotSpans = 1 << 0,
  // Discarding unused system pages is slower, because it involves walking all
  // freelists in all active slot spans of all buckets >= system page
  // size. It often frees a similar amount of memory to decommitting the empty
  // slot spans, though.
  PartitionPurgeDiscardUnusedSystemPages = 1 << 1,
  // Aggressively reclaim memory. This is meant to be used in low-memory
  // situations, not for periodic memory reclaiming.
  PartitionPurgeAggressiveReclaim = 1 << 2,
};

// Options struct used to configure PartitionRoot and PartitionAllocator.
struct PartitionOptions {
  enum class AlignedAlloc : uint8_t {
    // By default all allocations will be aligned to `base::kAlignment`,
    // likely to be 8B or 16B depending on platforms and toolchains.
    // AlignedAlloc() allows to enforce higher alignment.
    // This option determines whether it is supported for the partition.
    // Allowing AlignedAlloc() comes at a cost of disallowing extras in front
    // of the allocation.
    kDisallowed,
    kAllowed,
  };

  enum class ThreadCache : uint8_t {
    kDisabled,
    kEnabled,
  };

  enum class Quarantine : uint8_t {
    kDisallowed,
    kAllowed,
  };

  enum class Cookies : uint8_t {
    kDisallowed,
    kAllowed,
  };

  enum class RefCount : uint8_t {
    kDisallowed,
    kAllowed,
  };

  // Constructor to suppress aggregate initialization.
  constexpr PartitionOptions(AlignedAlloc aligned_alloc,
                             ThreadCache thread_cache,
                             Quarantine quarantine,
                             Cookies cookies,
                             RefCount ref_count)
      : aligned_alloc(aligned_alloc),
        thread_cache(thread_cache),
        quarantine(quarantine),
        cookies(cookies),
        ref_count(ref_count) {}

  AlignedAlloc aligned_alloc;
  ThreadCache thread_cache;
  Quarantine quarantine;
  Cookies cookies;
  RefCount ref_count;
};

// Never instantiate a PartitionRoot directly, instead use
// PartitionAllocator.
template <bool thread_safe>
struct BASE_EXPORT PartitionRoot {
  using SlotSpan = internal::SlotSpanMetadata<thread_safe>;
  using Page = internal::PartitionPage<thread_safe>;
  using Bucket = internal::PartitionBucket<thread_safe>;
  using SuperPageExtentEntry =
      internal::PartitionSuperPageExtentEntry<thread_safe>;
  using DirectMapExtent = internal::PartitionDirectMapExtent<thread_safe>;
  using ScopedGuard = internal::ScopedGuard<thread_safe>;
  using PCScan = internal::PCScan;

  // Defines whether objects should be quarantined for this root.
  enum class QuarantineMode : uint8_t {
    kAlwaysDisabled,
    kDisabledByDefault,
    kEnabled,
  } quarantine_mode = QuarantineMode::kAlwaysDisabled;

  // Defines whether the root should be scanned.
  enum class ScanMode : uint8_t {
    kDisabled,
    kEnabled,
  } scan_mode = ScanMode::kDisabled;

  // Flags accessed on fast paths.
  //
  // Careful! PartitionAlloc's performance is sensitive to its layout. Please
  // put the fast-path objects below, and the other ones further (see comment in
  // this file).
  bool with_thread_cache = false;
  const bool is_thread_safe = thread_safe;

  bool allow_aligned_alloc;
  bool allow_cookies;
  bool allow_ref_count;

  // Lazy commit should only be enabled on Windows, because commit charge is
  // only meaningful and limited on Windows. It affects performance on other
  // platforms and is simply not needed there due to OS supporting overcommit.
#if defined(OS_WIN)
  bool use_lazy_commit = true;
  static constexpr bool never_used_lazy_commit = false;
#else
  static constexpr bool use_lazy_commit = false;
  static constexpr bool never_used_lazy_commit = true;
#endif

#if !PARTITION_EXTRAS_REQUIRED
  // Teach the compiler that code can be optimized in builds that use no extras.
  static constexpr uint32_t extras_size = 0;
  static constexpr uint32_t extras_offset = 0;
#else
  uint32_t extras_size;
  uint32_t extras_offset;
#endif

  // Not used on the fastest path (thread cache allocations), but on the fast
  // path of the central allocator.
  internal::MaybeSpinLock<thread_safe> lock_;

  Bucket buckets[kNumBuckets] = {};
  Bucket sentinel_bucket;

  // All fields below this comment are not accessed on the fast path.
  bool initialized = false;

  // Bookkeeping.
  // - total_size_of_super_pages - total virtual address space for normal bucket
  //     super pages
  // - total_size_of_direct_mapped_pages - total virtual address space for
  //     direct-map regions
  // - total_size_of_committed_pages - total committed pages for slots (doesn't
  //     include metadata, bitmaps (if any), or any data outside or regions
  //     described in #1 and #2)
  // Invariant: total_size_of_committed_pages <
  //                total_size_of_super_pages +
  //                total_size_of_direct_mapped_pages.
  // Since all operations on these atomic variables have relaxed semantics, we
  // don't check this invariant with DCHECKs.
  std::atomic<size_t> total_size_of_committed_pages{0};
  std::atomic<size_t> total_size_of_super_pages{0};
  std::atomic<size_t> total_size_of_direct_mapped_pages{0};

  char* next_super_page = nullptr;
  char* next_partition_page = nullptr;
  char* next_partition_page_end = nullptr;
  SuperPageExtentEntry* current_extent = nullptr;
  SuperPageExtentEntry* first_extent = nullptr;
  DirectMapExtent* direct_map_list = nullptr;
  SlotSpan* global_empty_slot_span_ring[kMaxFreeableSpans] = {};
  int16_t global_empty_slot_span_ring_index = 0;

  // Integrity check = ~reinterpret_cast<uintptr_t>(this).
  uintptr_t inverted_self = 0;

  PartitionRoot() = default;
  explicit PartitionRoot(PartitionOptions opts) { Init(opts); }
  ~PartitionRoot();

  // Public API
  //
  // Allocates out of the given bucket. Properly, this function should probably
  // be in PartitionBucket, but because the implementation needs to be inlined
  // for performance, and because it needs to inspect SlotSpanMetadata,
  // it becomes impossible to have it in PartitionBucket as this causes a
  // cyclical dependency on SlotSpanMetadata function implementations.
  //
  // Moving it a layer lower couples PartitionRoot and PartitionBucket, but
  // preserves the layering of the includes.
  void Init(PartitionOptions);

  void ConfigureLazyCommit();

  ALWAYS_INLINE static bool IsValidSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSuperPage(char* super_page);
  ALWAYS_INLINE static PartitionRoot* FromPointerInNormalBuckets(char* ptr);

  ALWAYS_INLINE void IncreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecommitSystemPagesForData(
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void RecommitSystemPagesForData(
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE bool TryRecommitSystemPagesForData(
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  [[noreturn]] NOINLINE void OutOfMemory(size_t size);

  // Returns a pointer aligned on |alignment|, or nullptr.
  //
  // |alignment| has to be a power of two and a multiple of sizeof(void*) (as in
  // posix_memalign() for POSIX systems). The returned pointer may include
  // padding, and can be passed to |Free()| later.
  //
  // NOTE: This is incompatible with anything that adds extras before the
  // returned pointer, such as cookies (with DCHECK_IS_ON()), or reference
  // count.
  ALWAYS_INLINE void* AlignedAllocFlags(int flags,
                                        size_t alignment,
                                        size_t requested_size);

  ALWAYS_INLINE void* Alloc(size_t requested_size, const char* type_name);
  ALWAYS_INLINE void* AllocFlags(int flags,
                                 size_t requested_size,
                                 const char* type_name);
  // Same as |AllocFlags()|, but bypasses the allocator hooks.
  //
  // This is separate from AllocFlags() because other callers of AllocFlags()
  // should not have the extra branch checking whether the hooks should be
  // ignored or not. This is the same reason why |FreeNoHooks()|
  // exists. However, |AlignedAlloc()| and |Realloc()| have few callers, so
  // taking the extra branch in the non-malloc() case doesn't hurt. In addition,
  // for the malloc() case, the compiler correctly removes the branch, since
  // this is marked |ALWAYS_INLINE|.
  ALWAYS_INLINE void* AllocFlagsNoHooks(int flags, size_t requested_size);

  ALWAYS_INLINE void* Realloc(void* ptr, size_t newize, const char* type_name);
  // Overload that may return nullptr if reallocation isn't possible. In this
  // case, |ptr| remains valid.
  ALWAYS_INLINE void* TryRealloc(void* ptr,
                                 size_t new_size,
                                 const char* type_name);
  NOINLINE void* ReallocFlags(int flags,
                              void* ptr,
                              size_t new_size,
                              const char* type_name);
  ALWAYS_INLINE static void Free(void* ptr);
  // Same as |Free()|, bypasses the allocator hooks.
  ALWAYS_INLINE static void FreeNoHooks(void* ptr);
  // Immediately frees the pointer bypassing the quarantine.
  ALWAYS_INLINE void FreeNoHooksImmediate(void* ptr, SlotSpan* slot_span);

  ALWAYS_INLINE static size_t GetUsableSize(void* ptr);

  ALWAYS_INLINE size_t AllocationCapacityFromPtr(void* ptr) const;
  ALWAYS_INLINE size_t AllocationCapacityFromRequestedSize(size_t size) const;

  // Frees memory from this partition, if possible, by decommitting pages or
  // even etnire slot spans. |flags| is an OR of base::PartitionPurgeFlags.
  void PurgeMemory(int flags);

  void DumpStats(const char* partition_name,
                 bool is_light_dump,
                 PartitionStatsDumper* partition_stats_dumper);

  static uint16_t SizeToBucketIndex(size_t size);

  // Frees memory, with |slot_start| as returned by |RawAlloc()|.
  ALWAYS_INLINE void RawFree(void* slot_start);
  ALWAYS_INLINE void RawFree(void* slot_start, SlotSpan* slot_span);

  ALWAYS_INLINE void RawFreeWithThreadCache(void* slot_start,
                                            SlotSpan* slot_span);

  internal::ThreadCache* thread_cache_for_testing() const {
    return with_thread_cache ? internal::ThreadCache::Get() : nullptr;
  }
  size_t get_total_size_of_committed_pages() const {
    return total_size_of_committed_pages.load(std::memory_order_relaxed);
  }

  bool UseBRPPool() const {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    return allow_ref_count;
#else
    // This is counterintuitive, but when BRP isn't used, make all normal bucket
    // allocations fall into the same pool, and BRP pool is a good place for
    // that. PCScan requires this.
    return true;
#endif
  }

  ALWAYS_INLINE bool IsQuarantineAllowed() const {
    return quarantine_mode != QuarantineMode::kAlwaysDisabled;
  }

  ALWAYS_INLINE bool IsQuarantineEnabled() const {
    return quarantine_mode == QuarantineMode::kEnabled;
  }

  ALWAYS_INLINE bool IsScanEnabled() const {
    // Enabled scan implies enabled quarantine.
    PA_DCHECK(scan_mode != ScanMode::kEnabled || IsQuarantineEnabled());
    return scan_mode == ScanMode::kEnabled;
  }

  static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
  GetDirectMapMetadataAndGuardPagesSize() {
    // Because we need to fake a direct-map region to look like a super page, we
    // need to allocate a bunch of system pages more around the payload:
    // - The first few system pages are the partition page in which the super
    // page metadata is stored.
    // - We add a trailing guard page (one system page will suffice).
    return PartitionPageSize() + SystemPageSize();
  }

  static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
  GetDirectMapSlotSize(size_t raw_size) {
    // Caller must check that the size is not above the MaxDirectMapped()
    // limit before calling. This also guards against integer overflow in the
    // calculation here.
    PA_DCHECK(raw_size <= MaxDirectMapped());
    return bits::AlignUp(raw_size, SystemPageSize());
  }

  ALWAYS_INLINE size_t GetDirectMapReservedSize(size_t raw_size) {
    // Caller must check that the size is not above the MaxDirectMapped()
    // limit before calling. This also guards against integer overflow in the
    // calculation here.
    PA_DCHECK(raw_size <= MaxDirectMapped());
    // Align to allocation granularity. However, when 64-bit GigaCage is used,
    // the granularity is super page size.
    size_t alignment = PageAllocationGranularity();
#if defined(PA_HAS_64_BITS_POINTERS)
    if (features::IsPartitionAllocGigaCageEnabled()) {
      alignment = kSuperPageSize;
    }
#endif
    return bits::AlignUp(raw_size + GetDirectMapMetadataAndGuardPagesSize(),
                         alignment);
  }

  ALWAYS_INLINE size_t AdjustSize0IfNeeded(size_t size) const {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    // The minimum slot size is base::kAlignment. If |requested_size| is 0 and
    // there are extra before the allocation (which must be at least
    // kAlignment), then these extras will fill the slot, leading to returning a
    // pointer to the next slot. This is a problem, because e.g. FreeNoHooks()
    // or ReallocFlags() call SlotSpan::FromSlotInnerPtr(ptr) prior to
    // subtracting extras, thus getting a wrong, possibly non-existent, slot
    // span.
    //
    // Adjust the size to counteract it.
    //
    // Having any extras after the allocation nullifies the issue, so no need
    // for this adjustment in the REF_COUNT_AT_END_OF_ALLOCATION case. Same for
    // DCHECK_IS_ON(), but we prefer not to change codepaths between Release and
    // Debug.
    //
    // In theory, this can be further refined using run-time checks. No need
    // for this adjustment if |!extras_offset || (extras_size - extras_offset)|,
    // or IsPartitionAllocGigaCageEnabled() is false (because BackupRefPtr is
    // effectively disabled without GigaCage), but we prefer not to add more
    // checks, as this function may be called on hot paths.
    if (UNLIKELY(size == 0))
      return 1;
#else
    PA_DCHECK(!extras_offset || (extras_size - extras_offset));
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
    return size;
  }

  // Adjusts the size by adding extras. Also include the 0->1 adjustment if
  // needed.
  ALWAYS_INLINE size_t AdjustSizeForExtrasAdd(size_t size) const {
    size = AdjustSize0IfNeeded(size);
    PA_DCHECK(size + extras_size >= size);
    return size + extras_size;
  }

  // Adjusts the size by subtracing extras. Doesn't include the 0->1 adjustment,
  // which leads to an asymmetry with AdjustSizeForExtrasAdd, but callers of
  // AdjustSizeForExtrasSubtract either expect the adjustment to be included, or
  // are indifferent.
  ALWAYS_INLINE size_t AdjustSizeForExtrasSubtract(size_t size) const {
    return size - extras_size;
  }

  ALWAYS_INLINE void* AdjustPointerForExtrasAdd(void* ptr) const {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) +
                                   extras_offset);
  }

  ALWAYS_INLINE void* AdjustPointerForExtrasSubtract(void* ptr) const {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) -
                                   extras_offset);
  }

 private:
  // |buckets| has `kNumBuckets` elements, but we sometimes access it at index
  // `kNumBuckets`, which is occupied by the sentinel bucket. The correct layout
  // is enforced by a static_assert() in partition_root.cc, so this is
  // fine. However, UBSAN is correctly pointing out that there is an
  // out-of-bounds access, so disable it for these accesses.
  //
  // See crbug.com/1150772 for an instance of Clusterfuzz / UBSAN detecting
  // this.
  ALWAYS_INLINE const Bucket& NO_SANITIZE("undefined")
      bucket_at(size_t i) const {
    PA_DCHECK(i <= kNumBuckets);
    return buckets[i];
  }
  // Allocates memory, without initializing extras.
  //
  // - |flags| are as in AllocFlags().
  // - |raw_size| accommodates for extras on top of AllocFlags()'s
  //   |requested_size|.
  // - |usable_size| and |is_already_zeroed| are output only. |usable_size| is
  //   guaranteed to be larger or equal to AllocFlags()'s |requested_size|.
  ALWAYS_INLINE void* RawAlloc(Bucket* bucket,
                               int flags,
                               size_t raw_size,
                               size_t* usable_size,
                               bool* is_already_zeroed);
  ALWAYS_INLINE void* AllocFromBucket(Bucket* bucket,
                                      int flags,
                                      size_t raw_size,
                                      size_t* usable_size,
                                      bool* is_already_zeroed)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool TryReallocInPlace(void* ptr, SlotSpan* slot_span, size_t new_size);
  bool ReallocDirectMappedInPlace(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      size_t requested_size) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecommitEmptySlotSpans() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void RawFreeLocked(void* slot_start)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  friend class internal::ThreadCache;
};

namespace {

// Precalculate some shift and mask constants used in the hot path.
// Example: malloc(41) == 101001 binary.
// Order is 6 (1 << 6-1) == 32 is highest bit set.
// order_index is the next three MSB == 010 == 2.
// sub_order_index_mask is a mask for the remaining bits == 11 (masking to 01
// for the sub_order_index).
constexpr uint8_t OrderIndexShift(uint8_t order) {
  if (order < kNumBucketsPerOrderBits + 1)
    return 0;

  return order - (kNumBucketsPerOrderBits + 1);
}

constexpr size_t OrderSubIndexMask(uint8_t order) {
  if (order == kBitsPerSizeT)
    return static_cast<size_t>(-1) >> (kNumBucketsPerOrderBits + 1);

  return ((static_cast<size_t>(1) << order) - 1) >>
         (kNumBucketsPerOrderBits + 1);
}

#if defined(PA_HAS_64_BITS_POINTERS)
#define BITS_PER_SIZE_T 64
static_assert(kBitsPerSizeT == 64, "");
#else
#define BITS_PER_SIZE_T 32
static_assert(kBitsPerSizeT == 32, "");
#endif  // defined(PA_HAS_64_BITS_POINTERS)

constexpr uint8_t kOrderIndexShift[BITS_PER_SIZE_T + 1] = {
    OrderIndexShift(0),  OrderIndexShift(1),  OrderIndexShift(2),
    OrderIndexShift(3),  OrderIndexShift(4),  OrderIndexShift(5),
    OrderIndexShift(6),  OrderIndexShift(7),  OrderIndexShift(8),
    OrderIndexShift(9),  OrderIndexShift(10), OrderIndexShift(11),
    OrderIndexShift(12), OrderIndexShift(13), OrderIndexShift(14),
    OrderIndexShift(15), OrderIndexShift(16), OrderIndexShift(17),
    OrderIndexShift(18), OrderIndexShift(19), OrderIndexShift(20),
    OrderIndexShift(21), OrderIndexShift(22), OrderIndexShift(23),
    OrderIndexShift(24), OrderIndexShift(25), OrderIndexShift(26),
    OrderIndexShift(27), OrderIndexShift(28), OrderIndexShift(29),
    OrderIndexShift(30), OrderIndexShift(31), OrderIndexShift(32),
#if BITS_PER_SIZE_T == 64
    OrderIndexShift(33), OrderIndexShift(34), OrderIndexShift(35),
    OrderIndexShift(36), OrderIndexShift(37), OrderIndexShift(38),
    OrderIndexShift(39), OrderIndexShift(40), OrderIndexShift(41),
    OrderIndexShift(42), OrderIndexShift(43), OrderIndexShift(44),
    OrderIndexShift(45), OrderIndexShift(46), OrderIndexShift(47),
    OrderIndexShift(48), OrderIndexShift(49), OrderIndexShift(50),
    OrderIndexShift(51), OrderIndexShift(52), OrderIndexShift(53),
    OrderIndexShift(54), OrderIndexShift(55), OrderIndexShift(56),
    OrderIndexShift(57), OrderIndexShift(58), OrderIndexShift(59),
    OrderIndexShift(60), OrderIndexShift(61), OrderIndexShift(62),
    OrderIndexShift(63), OrderIndexShift(64)
#endif
};

constexpr size_t kOrderSubIndexMask[BITS_PER_SIZE_T + 1] = {
    OrderSubIndexMask(0),  OrderSubIndexMask(1),  OrderSubIndexMask(2),
    OrderSubIndexMask(3),  OrderSubIndexMask(4),  OrderSubIndexMask(5),
    OrderSubIndexMask(6),  OrderSubIndexMask(7),  OrderSubIndexMask(8),
    OrderSubIndexMask(9),  OrderSubIndexMask(10), OrderSubIndexMask(11),
    OrderSubIndexMask(12), OrderSubIndexMask(13), OrderSubIndexMask(14),
    OrderSubIndexMask(15), OrderSubIndexMask(16), OrderSubIndexMask(17),
    OrderSubIndexMask(18), OrderSubIndexMask(19), OrderSubIndexMask(20),
    OrderSubIndexMask(21), OrderSubIndexMask(22), OrderSubIndexMask(23),
    OrderSubIndexMask(24), OrderSubIndexMask(25), OrderSubIndexMask(26),
    OrderSubIndexMask(27), OrderSubIndexMask(28), OrderSubIndexMask(29),
    OrderSubIndexMask(30), OrderSubIndexMask(31), OrderSubIndexMask(32),
#if BITS_PER_SIZE_T == 64
    OrderSubIndexMask(33), OrderSubIndexMask(34), OrderSubIndexMask(35),
    OrderSubIndexMask(36), OrderSubIndexMask(37), OrderSubIndexMask(38),
    OrderSubIndexMask(39), OrderSubIndexMask(40), OrderSubIndexMask(41),
    OrderSubIndexMask(42), OrderSubIndexMask(43), OrderSubIndexMask(44),
    OrderSubIndexMask(45), OrderSubIndexMask(46), OrderSubIndexMask(47),
    OrderSubIndexMask(48), OrderSubIndexMask(49), OrderSubIndexMask(50),
    OrderSubIndexMask(51), OrderSubIndexMask(52), OrderSubIndexMask(53),
    OrderSubIndexMask(54), OrderSubIndexMask(55), OrderSubIndexMask(56),
    OrderSubIndexMask(57), OrderSubIndexMask(58), OrderSubIndexMask(59),
    OrderSubIndexMask(60), OrderSubIndexMask(61), OrderSubIndexMask(62),
    OrderSubIndexMask(63), OrderSubIndexMask(64)
#endif
};

}  // namespace

namespace internal {

// The class used to generate the bucket lookup table at compile-time.
class BucketIndexLookup final {
 public:
  ALWAYS_INLINE constexpr static size_t GetIndex(size_t size);

 private:
  constexpr BucketIndexLookup() {
    constexpr uint16_t sentinel_bucket_index = kNumBuckets;

    InitBucketSizes();

    uint16_t* bucket_index_ptr = &bucket_index_lookup_[0];
    uint16_t bucket_index = 0;

    for (uint8_t order = 0; order <= kBitsPerSizeT; ++order) {
      for (uint16_t j = 0; j < kNumBucketsPerOrder; ++j) {
        if (order < kMinBucketedOrder) {
          // Use the bucket of the finest granularity for malloc(0) etc.
          *bucket_index_ptr++ = 0;
        } else if (order > kMaxBucketedOrder) {
          *bucket_index_ptr++ = sentinel_bucket_index;
        } else {
          uint16_t valid_bucket_index = bucket_index;
          while (bucket_sizes_[valid_bucket_index] % kSmallestBucket)
            valid_bucket_index++;
          *bucket_index_ptr++ = valid_bucket_index;
          bucket_index++;
        }
      }
    }
    PA_DCHECK(bucket_index == kNumBuckets);
    PA_DCHECK(bucket_index_ptr == bucket_index_lookup_ + ((kBitsPerSizeT + 1) *
                                                          kNumBucketsPerOrder));
    // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
    // which tries to overflow to a non-existent order.
    *bucket_index_ptr = sentinel_bucket_index;
  }

  constexpr void InitBucketSizes() {
    size_t current_size = kSmallestBucket;
    size_t current_increment = kSmallestBucket >> kNumBucketsPerOrderBits;
    size_t* bucket_size = &bucket_sizes_[0];
    for (size_t i = 0; i < kNumBucketedOrders; ++i) {
      for (size_t j = 0; j < kNumBucketsPerOrder; ++j) {
        *bucket_size = current_size;
        // Disable pseudo buckets so that touching them faults.
        current_size += current_increment;
        ++bucket_size;
      }
      current_increment <<= 1;
    }
  }

  size_t bucket_sizes_[kNumBuckets]{};
  // The bucket lookup table lets us map a size_t to a bucket quickly.
  // The trailing +1 caters for the overflow case for very large allocation
  // sizes.  It is one flat array instead of a 2D array because in the 2D
  // world, we'd need to index array[blah][max+1] which risks undefined
  // behavior.
  uint16_t
      bucket_index_lookup_[((kBitsPerSizeT + 1) * kNumBucketsPerOrder) + 1]{};
};

// static
ALWAYS_INLINE constexpr size_t BucketIndexLookup::GetIndex(size_t size) {
  // This forces the bucket table to be constant-initialized and immediately
  // materialized in the binary.
  constexpr BucketIndexLookup lookup{};
  const uint8_t order = kBitsPerSizeT - bits::CountLeadingZeroBitsSizeT(size);
  // The order index is simply the next few bits after the most significant
  // bit.
  const size_t order_index =
      (size >> kOrderIndexShift[order]) & (kNumBucketsPerOrder - 1);
  // And if the remaining bits are non-zero we must bump the bucket up.
  const size_t sub_order_index = size & kOrderSubIndexMask[order];
  const uint16_t index =
      lookup.bucket_index_lookup_[(order << kNumBucketsPerOrderBits) +
                                  order_index + !!sub_order_index];
  PA_DCHECK(index <= kNumBuckets);  // Last one is the sentinel bucket.
  return index;
}

// Gets the SlotSpanMetadata object of the slot span that contains |ptr|. It's
// used with intention to do obtain the slot size.
//
// CAUTION! Use only for normal buckets. Using on direct-mapped allocations may
// lead to undefined behavior.
template <bool thread_safe>
ALWAYS_INLINE internal::SlotSpanMetadata<thread_safe>*
PartitionAllocGetSlotSpanForSizeQuery(void* ptr) {
  // TODO(bartekn): Add a "is in normal buckets" DCHECK.

  // No need to lock here. Only |ptr| being freed by another thread could
  // cause trouble, and the caller is responsible for that not happening.
  auto* slot_span =
      internal::SlotSpanMetadata<thread_safe>::FromSlotInnerPtr(ptr);
  // TODO(palmer): See if we can afford to make this a CHECK.
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
  return slot_span;
}

#if BUILDFLAG(USE_BACKUP_REF_PTR)

// Gets the pointer to the beginning of the allocated slot.
//
// This isn't a general purpose function, it is used specifically for obtaining
// BackupRefPtr's ref-count. The caller is responsible for ensuring that the
// ref-count is in place for this allocation.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
ALWAYS_INLINE void* PartitionAllocGetSlotStart(void* ptr) {
  // Adjust to support pointers right past the end of an allocation, which in
  // some cases appear to point outside the designated allocation slot.
  //
  // If ref-count is present before the allocation, then adjusting a valid
  // pointer down will not cause us to go down to the previous slot. If
  // ref-count is present after the allocation, then adjust no adjustment is
  // needed (and likely wouldn't be correct as there is a risk of going down to
  // the previous slot). Either way, kPartitionPastAllocationAdjustment takes
  // care of that detail.
  ptr = reinterpret_cast<char*>(ptr) - kPartitionPastAllocationAdjustment;

  internal::DCheckIfManagedByPartitionAllocBRPPool(ptr);

  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          ptr);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->allow_ref_count);

  // Get the offset from the beginning of the slot span.
  uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t slot_span_start = reinterpret_cast<uintptr_t>(
      internal::SlotSpanMetadata<internal::ThreadSafe>::ToSlotSpanStartPtr(
          slot_span));
  size_t offset_in_slot_span = ptr_addr - slot_span_start;

  auto* bucket = slot_span->bucket;
  return reinterpret_cast<void*>(
      slot_span_start +
      bucket->slot_size * bucket->GetSlotNumber(offset_in_slot_span));
}

// Checks whether a given pointer stays within the same allocation slot after
// modification.
//
// This isn't a general purpose function. The caller is responsible for ensuring
// that the ref-count is in place for this allocation.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
ALWAYS_INLINE bool PartitionAllocIsValidPtrDelta(void* ptr, ptrdiff_t delta) {
  // Required for pointers right past an allocation. See
  // |PartitionAllocGetSlotStart()|.
  void* adjusted_ptr =
      reinterpret_cast<char*>(ptr) - kPartitionPastAllocationAdjustment;

  internal::DCheckIfManagedByPartitionAllocBRPPool(adjusted_ptr);
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          adjusted_ptr);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->allow_ref_count);

  uintptr_t user_data_start = reinterpret_cast<uintptr_t>(
      root->AdjustPointerForExtrasAdd(PartitionAllocGetSlotStart(ptr)));
  size_t user_data_size = slot_span->GetUsableSize(root);
  uintptr_t new_ptr = reinterpret_cast<uintptr_t>(ptr) + delta;

  return user_data_start <= new_ptr &&
         // We use "greater then or equal" below because we want to include
         // pointers right past the end of an allocation.
         new_ptr <= user_data_start + user_data_size;
}

// TODO(glazunov): Simplify the function once the non-thread-safe PartitionRoot
// is no longer used.
ALWAYS_INLINE void PartitionAllocFreeForRefCounting(void* slot_start) {
  PA_DCHECK(!internal::PartitionRefCountPointer(slot_start)->IsAlive());

  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStartPtr(slot_start);
  auto* root = PartitionRoot<ThreadSafe>::FromSlotSpan(slot_span);
  // PartitionRefCount is required to be allocated inside a `PartitionRoot` that
  // supports reference counts.
  PA_DCHECK(root->allow_ref_count);

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(slot_start, kFreedByte,
         slot_span->GetUtilizedSlotSize()
#if BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#endif

  if (root->is_thread_safe) {
    root->RawFreeWithThreadCache(slot_start, slot_span);
    return;
  }

  auto* non_thread_safe_slot_span =
      reinterpret_cast<SlotSpanMetadata<NotThreadSafe>*>(slot_span);
  auto* non_thread_safe_root =
      reinterpret_cast<PartitionRoot<NotThreadSafe>*>(root);
  non_thread_safe_root->RawFreeWithThreadCache(slot_start,
                                               non_thread_safe_slot_span);
}
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

}  // namespace internal

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFromBucket(
    Bucket* bucket,
    int flags,
    size_t raw_size,
    size_t* usable_size,
    bool* is_already_zeroed) {
  SlotSpan* slot_span = bucket->active_slot_spans_head;
  // Check that this slot span is neither full nor freed.
  PA_DCHECK(slot_span);
  PA_DCHECK(slot_span->num_allocated_slots >= 0);

  void* slot_start = slot_span->freelist_head;
  if (LIKELY(slot_start)) {
    *is_already_zeroed = false;
    // This is a fast path, so avoid calling GetUsableSize() on Release builds
    // as it is more costly. Copy its small bucket path instead.
    *usable_size = AdjustSizeForExtrasSubtract(bucket->slot_size);
    PA_DCHECK(*usable_size == slot_span->GetUsableSize(this));

    // If these DCHECKs fire, you probably corrupted memory. TODO(palmer): See
    // if we can afford to make these CHECKs.
    PA_DCHECK(IsValidSlotSpan(slot_span));

    // All large allocations must go through the slow path to correctly update
    // the size metadata.
    PA_DCHECK(!slot_span->CanStoreRawSize());
    PA_DCHECK(!slot_span->bucket->is_direct_mapped());
    internal::PartitionFreelistEntry* new_head =
        slot_span->freelist_head->GetNext();
    slot_span->SetFreelistHead(new_head);
    slot_span->num_allocated_slots++;

    PA_DCHECK(slot_span->bucket == bucket);
  } else {
    slot_start =
        bucket->SlowPathAlloc(this, flags, raw_size, is_already_zeroed);
    // TODO(palmer): See if we can afford to make this a CHECK.
    PA_DCHECK(!slot_start ||
              IsValidSlotSpan(SlotSpan::FromSlotStartPtr(slot_start)));

    if (UNLIKELY(!slot_start))
      return nullptr;

    slot_span = SlotSpan::FromSlotStartPtr(slot_start);
    // For direct mapped allocations, |bucket| is the sentinel.
    PA_DCHECK((slot_span->bucket == bucket) ||
              (slot_span->bucket->is_direct_mapped() &&
               (bucket == &sentinel_bucket)));

    *usable_size = slot_span->GetUsableSize(this);
  }
  PA_DCHECK(slot_span->GetUtilizedSlotSize() <= slot_span->bucket->slot_size);
  return slot_start;
}

// static
template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::Free(void* ptr) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  free(ptr);
#else
  if (UNLIKELY(!ptr))
    return;

  if (PartitionAllocHooks::AreHooksEnabled()) {
    PartitionAllocHooks::FreeObserverHookIfEnabled(ptr);
    if (PartitionAllocHooks::FreeOverrideHookIfEnabled(ptr))
      return;
  }

  FreeNoHooks(ptr);
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
}

// static
template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeNoHooks(void* ptr) {
  if (UNLIKELY(!ptr))
    return;

    // On Android, malloc() interception is more fragile than on other
    // platforms, as we use wrapped symbols. However, the GigaCage allows us to
    // quickly tell that a pointer was allocated with PartitionAlloc.
    //
    // This is a crash to detect imperfect symbol interception. However, we can
    // forward allocations we don't own to the system malloc() implementation in
    // these rare cases, assuming that some remain.
#if defined(OS_ANDROID) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // GigaCage is always enabled on Android and is needed for PA_CHECK below.
  PA_DCHECK(features::IsPartitionAllocGigaCageEnabled());
  PA_CHECK(IsManagedByPartitionAllocBRPPool(ptr) ||
           IsManagedByPartitionAllocNonBRPPool(ptr));
#endif

  // Call FromSlotInnerPtr instead of FromSlotStartPtr because the pointer
  // hasn't been adjusted yet.
  SlotSpan* slot_span = SlotSpan::FromSlotInnerPtr(ptr);
  // TODO(palmer): See if we can afford to make this a CHECK.
  PA_DCHECK(IsValidSlotSpan(slot_span));
  auto* root = FromSlotSpan(slot_span);

  // TODO(bikineev): Change the first condition to LIKELY once PCScan is enabled
  // by default.
  if (UNLIKELY(root->IsQuarantineEnabled()) &&
      LIKELY(!slot_span->bucket->is_direct_mapped())) {
    PCScan::Instance().MoveToQuarantine(ptr, slot_span->bucket->slot_size);
    return;
  }

  root->FreeNoHooksImmediate(ptr, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeNoHooksImmediate(
    void* ptr,
    SlotSpan* slot_span) {
  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/ref-count management
  // - Before the "raw" allocator.
  //
  // On the deallocation side:
  // 1. Check cookies/ref-count, adjust the pointer
  // 2. Deallocation
  //   a. Return to the thread cache if possible. If it succeeds, return.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  PA_DCHECK(ptr);
  PA_DCHECK(slot_span);
  PA_DCHECK(IsValidSlotSpan(slot_span));

  // |ptr| points after the ref-count and the cookie.
  //
  // Layout inside the slot:
  //  <------extras----->                  <-extras->
  //  <--------------utilized_slot_size------------->
  //                    <----usable_size--->
  //  |[refcnt]|[cookie]|...data...|[empty]|[cookie]|[unused]|
  //                    ^
  //                   ptr
  //
  // Note: ref-count and cookies can be 0-sized.
  //
  // For more context, see the other "Layout inside the slot" comment below.
#if EXPENSIVE_DCHECKS_ARE_ON() || PA_ZERO_RANDOMLY_ON_FREE
  const size_t utilized_slot_size = slot_span->GetUtilizedSlotSize();
#endif
#if BUILDFLAG(USE_BACKUP_REF_PTR) || DCHECK_IS_ON()
  const size_t usable_size = slot_span->GetUsableSize(this);
#endif
  void* slot_start = AdjustPointerForExtrasSubtract(ptr);

#if DCHECK_IS_ON()
  if (allow_cookies) {
    // Verify 2 cookies surrounding the allocated region.
    // If these asserts fire, you probably corrupted memory.
    char* char_ptr = static_cast<char*>(ptr);
    internal::PartitionCookieCheckValue(char_ptr - internal::kCookieSize);
    internal::PartitionCookieCheckValue(char_ptr + usable_size);
  }
#endif

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (allow_ref_count) {
    PA_DCHECK(features::IsPartitionAllocGigaCageEnabled());
    if (LIKELY(!slot_span->bucket->is_direct_mapped())) {
      auto* ref_count = internal::PartitionRefCountPointer(slot_start);
      // If there are no more references to the allocation, it can be freed
      // immediately. Otherwise, defer the operation and zap the memory to turn
      // potential use-after-free issues into unexploitable crashes.
      if (UNLIKELY(!ref_count->IsAliveWithNoKnownRefs()))
        internal::SecureMemset(ptr, kQuarantinedByte, usable_size);

      if (UNLIKELY(!(ref_count->ReleaseFromAllocator())))
        return;
    }
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(slot_start, kFreedByte,
         utilized_slot_size
#if BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#elif PA_ZERO_RANDOMLY_ON_FREE
  // `memset` only once in a while: we're trading off safety for time
  // efficiency.
  if (UNLIKELY(internal::RandomPeriod()) &&
      !slot_span->bucket->is_direct_mapped()) {
    internal::SecureMemset(slot_start, 0,
                           utilized_slot_size
#if BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)
                               - sizeof(internal::PartitionRefCount)
#endif
    );
  }
#endif

  RawFreeWithThreadCache(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(void* slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
  RawFree(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(void* slot_start,
                                                       SlotSpan* slot_span) {
  internal::DeferredUnmap deferred_unmap;
  {
    ScopedGuard guard{lock_};
    deferred_unmap = slot_span->Free(slot_start);
  }
  deferred_unmap.Run();
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeWithThreadCache(
    void* slot_start,
    SlotSpan* slot_span) {
  // TLS access can be expensive, do a cheap local check first.
  //
  // Also the thread-unsafe variant doesn't have a use for a thread cache, so
  // make it statically known to the compiler.
  //
  // LIKELY: performance-sensitive thread-safe partitions have a thread cache,
  // direct-mapped allocations are uncommon.
  if (thread_safe &&
      LIKELY(with_thread_cache && !slot_span->bucket->is_direct_mapped())) {
    PA_DCHECK(slot_span->bucket >= this->buckets &&
              slot_span->bucket <= &this->sentinel_bucket);
    size_t bucket_index = slot_span->bucket - this->buckets;
    auto* thread_cache = internal::ThreadCache::Get();
    if (LIKELY(internal::ThreadCache::IsValid(thread_cache) &&
               thread_cache->MaybePutInCache(slot_start, bucket_index))) {
      return;
    }
  }

  RawFree(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeLocked(void* slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
  auto deferred_unmap = slot_span->Free(slot_start);
  PA_DCHECK(!deferred_unmap.ptr);  // Only used with bucketed allocations.
  deferred_unmap.Run();
}

// static
template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::IsValidSlotSpan(
    SlotSpan* slot_span) {
  PartitionRoot* root = FromSlotSpan(slot_span);
  return root->inverted_self == ~reinterpret_cast<uintptr_t>(root);
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromSlotSpan(SlotSpan* slot_span) {
  auto* extent_entry = reinterpret_cast<SuperPageExtentEntry*>(
      reinterpret_cast<uintptr_t>(slot_span) & SystemPageBaseMask());
  return extent_entry->root;
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromSuperPage(char* super_page) {
  auto* extent_entry = reinterpret_cast<SuperPageExtentEntry*>(
      internal::PartitionSuperPageToMetadataArea(super_page));
  PartitionRoot* root = extent_entry->root;
  PA_DCHECK(root->inverted_self == ~reinterpret_cast<uintptr_t>(root));
  return root;
}

// CAUTION! Use only for normal buckets. Using on direct-mapped allocations may
// lead to undefined behavior.
template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromPointerInNormalBuckets(char* ptr) {
  // TODO(bartekn): Add a "is in normal buckets" DCHECK.
  char* super_page = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(ptr) &
                                             kSuperPageBaseMask);
  return FromSuperPage(super_page);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::IncreaseCommittedPages(
    size_t len) {
  total_size_of_committed_pages.fetch_add(len, std::memory_order_relaxed);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::DecreaseCommittedPages(
    size_t len) {
  total_size_of_committed_pages.fetch_sub(len, std::memory_order_relaxed);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::DecommitSystemPagesForData(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  DecommitSystemPages(address, length, accessibility_disposition);
  DecreaseCommittedPages(length);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RecommitSystemPagesForData(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  RecommitSystemPages(address, length, PageReadWrite,
                      accessibility_disposition);
  IncreaseCommittedPages(length);
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::TryRecommitSystemPagesForData(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  bool ok = TryRecommitSystemPages(address, length, PageReadWrite,
                                   accessibility_disposition);
  if (ok)
    IncreaseCommittedPages(length);

  return ok;
}

// static
// Returns the size available to the app. It can be equal or higher than the
// requested size. If higher, the overage won't exceed what's actually usable
// by the app without a risk of running out of an allocated region or into
// PartitionAlloc's internal data. Used as malloc_usable_size.
template <bool thread_safe>
ALWAYS_INLINE size_t PartitionRoot<thread_safe>::GetUsableSize(void* ptr) {
  auto* slot_span = SlotSpan::FromSlotInnerPtr(ptr);
  auto* root = FromSlotSpan(slot_span);
  return slot_span->GetUsableSize(root);
}

// Return the capacity of the underlying slot (adjusted for extras). This
// doesn't mean this capacity is readily available. It merely means that if
// a new allocation (or realloc) happened with that returned value, it'd use
// the same amount of underlying memory.
// CAUTION! For direct-mapped allocation, |ptr| has to be within the first
// partition page.
template <bool thread_safe>
ALWAYS_INLINE size_t
PartitionRoot<thread_safe>::AllocationCapacityFromPtr(void* ptr) const {
  ptr = AdjustPointerForExtrasSubtract(ptr);
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<thread_safe>(ptr);
  size_t size = AdjustSizeForExtrasSubtract(slot_span->bucket->slot_size);
  return size;
}

// static
template <bool thread_safe>
ALWAYS_INLINE uint16_t
PartitionRoot<thread_safe>::SizeToBucketIndex(size_t size) {
  return internal::BucketIndexLookup::GetIndex(size);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFlags(
    int flags,
    size_t requested_size,
    const char* type_name) {
  PA_DCHECK(flags < PartitionAllocLastFlag << 1);
  PA_DCHECK((flags & PartitionAllocNoHooks) == 0);  // Internal only.
  PA_DCHECK(initialized);

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(requested_size, flags);
  const bool zero_fill = flags & PartitionAllocZeroFill;
  void* result = zero_fill ? calloc(1, requested_size) : malloc(requested_size);
  PA_CHECK(result || flags & PartitionAllocReturnNull);
  return result;
#else
  PA_DCHECK(initialized);
  void* ret = nullptr;
  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  if (UNLIKELY(hooks_enabled)) {
    if (PartitionAllocHooks::AllocationOverrideHookIfEnabled(
            &ret, flags, requested_size, type_name)) {
      PartitionAllocHooks::AllocationObserverHookIfEnabled(ret, requested_size,
                                                           type_name);
      return ret;
    }
  }

  ret = AllocFlagsNoHooks(flags, requested_size);

  if (UNLIKELY(hooks_enabled)) {
    PartitionAllocHooks::AllocationObserverHookIfEnabled(ret, requested_size,
                                                         type_name);
  }

  return ret;
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFlagsNoHooks(
    int flags,
    size_t requested_size) {
  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/ref-count management
  // - Before the "raw" allocator.
  //
  // That is, the general allocation flow is:
  // 1. Adjustment of requested size to make room for extras
  // 2. Allocation:
  //   a. Call to the thread cache, if it succeeds, go to step 3.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  // 3. Handle cookies/ref-count, zero allocation if required

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  PA_CHECK(raw_size >= requested_size);  // check for overflows

  uint16_t bucket_index = SizeToBucketIndex(raw_size);
  size_t usable_size;
  bool is_already_zeroed = false;
  void* slot_start = nullptr;
  size_t slot_size;

  // PCScan safepoint. Call before trying to allocate from cache.
  if (IsQuarantineEnabled())
    PCScan::Instance().JoinScanIfNeeded();

  // !thread_safe => !with_thread_cache, but adding the condition allows the
  // compiler to statically remove this branch for the thread-unsafe variant.
  //
  // LIKELY: performance-sensitive partitions are either thread-unsafe or use
  // the thread cache.
  if (thread_safe && LIKELY(with_thread_cache)) {
    auto* tcache = internal::ThreadCache::Get();
    if (UNLIKELY(!internal::ThreadCache::IsValid(tcache))) {
      if (internal::ThreadCache::IsTombstone(tcache)) {
        // Thread is being terminated, don't try to use the thread cache, and
        // don't try to resurrect it.
      } else {
        // There is no per-thread ThreadCache allocated here yet, and this
        // partition has a thread cache, allocate a new one.
        //
        // The thread cache allocation itself will not reenter here, as it
        // sidesteps the thread cache by using placement new and
        // |RawAlloc()|. However, internally to libc, allocations may happen to
        // create a new TLS variable. This would end up here again, which is not
        // what we want (and likely is not supported by libc).
        //
        // To avoid this sort of reentrancy, temporarily set this partition as
        // not supporting a thread cache. so that reentering allocations will
        // not end up allocating a thread cache. This value may be seen by other
        // threads as well, in which case a few allocations will not use the
        // thread cache. As it is purely an optimization, this is not a
        // correctness issue.
        //
        // Note that there is no deadlock or data inconsistency concern, since
        // we do not hold the lock, and has such haven't touched any internal
        // data.
        with_thread_cache = false;
        tcache = internal::ThreadCache::Create(this);
        with_thread_cache = true;

        // Cache is created empty, but at least this will trigger batch fill,
        // which may be useful, and we are already in a slow path anyway (first
        // small allocation of this thread).
        slot_start = tcache->GetFromCache(bucket_index, &slot_size);
      }
    } else {
      slot_start = tcache->GetFromCache(bucket_index, &slot_size);
    }

    // LIKELY: median hit rate in the thread cache is 95%, from metrics.
    if (LIKELY(slot_start)) {
      // This follows the logic of SlotSpanMetadata::GetUsableSize for small
      // buckets, which is too expensive to call here.
      // Keep it in sync!
      usable_size = AdjustSizeForExtrasSubtract(slot_size);

#if DCHECK_IS_ON()
      // Make sure that the allocated pointer comes from the same place it would
      // for a non-thread cache allocation.
      SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
      PA_DCHECK(IsValidSlotSpan(slot_span));
      PA_DCHECK(slot_span->bucket == &bucket_at(bucket_index));
      PA_DCHECK(slot_span->bucket->slot_size == slot_size);
      PA_DCHECK(usable_size == slot_span->GetUsableSize(this));
      // All large allocations must go through the RawAlloc path to correctly
      // set |usable_size|.
      PA_DCHECK(!slot_span->CanStoreRawSize());
      PA_DCHECK(!slot_span->bucket->is_direct_mapped());
#endif
    } else {
      slot_start = RawAlloc(buckets + bucket_index, flags, raw_size,
                            &usable_size, &is_already_zeroed);
    }
  } else {
    slot_start = RawAlloc(buckets + bucket_index, flags, raw_size, &usable_size,
                          &is_already_zeroed);
  }

  if (UNLIKELY(!slot_start))
    return nullptr;

  // Layout inside the slot:
  //  |[refcnt]|[cookie]|...data...|[empty]|[cookie]|[unused]|
  //                    <---(a)---->
  //                    <-------(b)-------->
  //  <-------(c)------->                  <--(c)--->
  //  <-------------(d)------------>   +   <--(d)--->
  //  <---------------------(e)--------------------->
  //  <-------------------------(f)-------------------------->
  //   (a) requested_size
  //   (b) usable_size
  //   (c) extras
  //   (d) raw_size
  //   (e) utilized_slot_size
  //   (f) slot_size
  //
  // - Ref-count may or may not exist in the slot, depending on CheckedPtr
  //   implementation.
  // - Cookies exist only when DCHECK is on.
  // - Think of raw_size as the minimum size required internally to satisfy
  //   the allocation request (i.e. requested_size + extras)
  // - Note, at most one "empty" or "unused" space can occur at a time. It
  //   occurs when slot_size is larger than raw_size. "unused" applies only to
  //   large allocations (direct-mapped and single-slot slot spans) and "empty"
  //   only to small allocations.
  //   Why either-or, one might ask? We make an effort to put the trailing
  //   cookie as close to data as possible to catch overflows (often
  //   off-by-one), but that's possible only if we have enough space in metadata
  //   to save raw_size, i.e. only for large allocations. For small allocations,
  //   we have no other choice than putting the cookie at the very end of the
  //   slot, thus creating the "empty" space.
  //
  // If BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION) is true, Layout inside the
  // slot of small buckets:
  //  |[cookie]|...data...|[empty]|[cookie]|[refcnt]|
  //           <---(a)---->
  //           <-------(b)-------->
  //  <--(c)--->                  <-------(c)------->
  //  <---------(d)------->   +   <-------(d)------->
  //  <------------------(e)------------------------>
  //  <------------------(f)------------------------>
  //
  // If the slot start address is not SystemPageSize() aligned (this also means,
  // the slot size is small), [refcnt] of this slot is stored at the end of
  // the previous slot. So this makes us to obtain refcount address with slot
  // start address minus sizeof(refcount).
  // If the slot start address is SystemPageSize() aligned (regarding single
  // slot span, the slot start address is always SystemPage size-aligned),
  // [refcnt] is stored in refcount bitmap placed after SuperPage metadata.
  // However, the space for refcnt is still reserved at the end of slot, even
  // though redundant. Because, regarding not single slot span, it is a little
  // difficult to change usable_size if refcnt serves the slot in the next
  // system page.
  // TODO(tasak): we don't need to add/subtract sizeof(refcnt) to requested size
  // in single slot span case.

  // The value given to the application is just after the ref-count and cookie,
  // or the cookie (BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION) is true).
  void* ret = AdjustPointerForExtrasAdd(slot_start);

#if DCHECK_IS_ON()
  // Surround the region with 2 cookies.
  if (allow_cookies) {
    char* char_ret = static_cast<char*>(ret);
    internal::PartitionCookieWriteValue(char_ret - internal::kCookieSize);
    internal::PartitionCookieWriteValue(char_ret + usable_size);
  }
#endif

  // Fill the region kUninitializedByte (on debug builds, if not requested to 0)
  // or 0 (if requested and not 0 already).
  bool zero_fill = flags & PartitionAllocZeroFill;
  // LIKELY: operator new() calls malloc(), not calloc().
  if (LIKELY(!zero_fill)) {
    // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
    memset(ret, kUninitializedByte, usable_size);
#endif
  } else if (!is_already_zeroed) {
    memset(ret, 0, usable_size);
  }

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  bool is_direct_mapped = raw_size > kMaxBucketed;
  // LIKELY: Direct mapped allocations are large and rare.
  if (allow_ref_count && LIKELY(!is_direct_mapped)) {
    PA_DCHECK(features::IsPartitionAllocGigaCageEnabled());
    new (internal::PartitionRefCountPointer(slot_start))
        internal::PartitionRefCount();
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::RawAlloc(
    Bucket* bucket,
    int flags,
    size_t raw_size,
    size_t* usable_size,
    bool* is_already_zeroed) {
  internal::ScopedGuard<thread_safe> guard{lock_};
  return AllocFromBucket(bucket, flags, raw_size, usable_size,
                         is_already_zeroed);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AlignedAllocFlags(
    int flags,
    size_t alignment,
    size_t requested_size) {
  // Aligned allocation support relies on the natural alignment guarantees of
  // PartitionAlloc. Specifically, it relies on the fact that slot spans are
  // partition-page aligned, and slots within are aligned to slot size, from
  // the beginning of the span. As a conseqneuce, allocations of sizes that are
  // a power of two, up to partition page size, will always be aligned to its
  // size. The code below adjusts the request size to be a power of two.
  // TODO(bartekn): Handle requests larger than partition page.
  //
  // Extras before the allocation are forbidden as they shift the returned
  // allocation from the beginning of the slot, thus messing up alignment.
  // Extras after the allocation are acceptable, but they have to be taken into
  // account in the request size calculation to avoid crbug.com/1185484.
  PA_DCHECK(allow_aligned_alloc);
  PA_DCHECK(!extras_offset);
  // This is mandated by |posix_memalign()|, so should never fire.
  PA_CHECK(base::bits::IsPowerOfTwo(alignment));

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  // Handle cases such as size = 16, alignment = 64.
  // Wastes memory when a large alignment is requested with a small size, but
  // this is hard to avoid, and should not be too common.
  if (UNLIKELY(raw_size < alignment)) {
    raw_size = alignment;
  } else {
    // PartitionAlloc only guarantees alignment for power-of-two sized
    // allocations. To make sure this applies here, round up the allocation
    // size.
    raw_size = static_cast<size_t>(1)
               << (sizeof(size_t) * 8 -
                   base::bits::CountLeadingZeroBits(raw_size - 1));
  }
  PA_DCHECK(base::bits::IsPowerOfTwo(raw_size));
  // Adjust back, because AllocFlagsNoHooks/Alloc will adjust it again.
  size_t adjusted_size = AdjustSizeForExtrasSubtract(raw_size);

  // Overflow check. adjusted_size must be larger or equal to requested_size.
  if (UNLIKELY(adjusted_size < requested_size)) {
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    // OutOfMemoryDeathTest.AlignedAlloc requires
    // base::TerminateBecauseOutOfMemory (invoked by
    // PartitionExcessiveAllocationSize).
    internal::PartitionExcessiveAllocationSize(requested_size);
    // internal::PartitionExcessiveAllocationSize(size) causes OOM_CRASH.
    NOTREACHED();
  }

  bool no_hooks = flags & PartitionAllocNoHooks;
  void* ptr =
      no_hooks ? AllocFlagsNoHooks(0, adjusted_size) : Alloc(adjusted_size, "");

  // |alignment| is a power of two, but the compiler doesn't necessarily know
  // that. A regular % operation is very slow, make sure to use the equivalent,
  // faster form.
  PA_CHECK(!(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)));

  return ptr;
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::Alloc(size_t requested_size,
                                                      const char* type_name) {
  return AllocFlags(0, requested_size, type_name);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::Realloc(void* ptr,
                                                        size_t new_size,
                                                        const char* type_name) {
  return ReallocFlags(0, ptr, new_size, type_name);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::TryRealloc(
    void* ptr,
    size_t new_size,
    const char* type_name) {
  return ReallocFlags(PartitionAllocReturnNull, ptr, new_size, type_name);
}

// Return the capacity of the underlying slot (adjusted for extras) that'd be
// used to satisfy a request of |size|. This doesn't mean this capacity would be
// readily available. It merely means that if an allocation happened with that
// returned value, it'd use the same amount of underlying memory as the
// allocation with |size|.
template <bool thread_safe>
ALWAYS_INLINE size_t
PartitionRoot<thread_safe>::AllocationCapacityFromRequestedSize(
    size_t size) const {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  return size;
#else
  PA_DCHECK(PartitionRoot<thread_safe>::initialized);
  size = AdjustSizeForExtrasAdd(size);
  auto& bucket = bucket_at(SizeToBucketIndex(size));
  PA_DCHECK(!bucket.slot_size || bucket.slot_size >= size);
  PA_DCHECK(!(bucket.slot_size % kSmallestBucket));

  if (LIKELY(!bucket.is_direct_mapped())) {
    size = bucket.slot_size;
  } else if (size > MaxDirectMapped()) {
    // Too large to allocate => return the size unchanged.
  } else {
    size = GetDirectMapSlotSize(size);
  }
  size = AdjustSizeForExtrasSubtract(size);
  return size;
#endif
}

using ThreadSafePartitionRoot = PartitionRoot<internal::ThreadSafe>;
using ThreadUnsafePartitionRoot = PartitionRoot<internal::NotThreadSafe>;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
