// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan_internal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/raceful_worklist.h"
#include "base/allocator/partition_allocator/starscan/scan_loop.h"
#include "base/allocator/partition_allocator/starscan/snapshot.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/starscan/stats_collector.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {
namespace internal {

[[noreturn]] BASE_EXPORT NOINLINE NOT_TAIL_CALLED void DoubleFreeAttempt() {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

namespace {

#if DCHECK_IS_ON() && defined(OS_LINUX)
// Currently, check reentracy only on Linux. On Android TLS is emulated by the
// runtime lib, which can allocate and therefore cause reentrancy.
struct ReentrantScannerGuard final {
 public:
  ReentrantScannerGuard() {
    PA_CHECK(!guard_);
    guard_ = true;
  }
  ~ReentrantScannerGuard() { guard_ = false; }

 private:
  // Since this variable has hidden visibility (not referenced by other DSOs),
  // assume that thread_local works on all supported architectures.
  static thread_local size_t guard_;
};
thread_local size_t ReentrantScannerGuard::guard_ = 0;
#else
struct [[maybe_unused]] ReentrantScannerGuard final{};
#endif

#if PA_STARSCAN_USE_CARD_TABLE
// Bytemap that represent regions (cards) that contain quarantined objects.
// A single PCScan cycle consists of the following steps:
// 1) clearing (memset quarantine + marking cards that contain quarantine);
// 2) scanning;
// 3) sweeping (freeing + unmarking cards that contain freed objects).
// Marking cards on step 1) ensures that the card table stays in the consistent
// state while scanning. Unmarking on the step 3) ensures that unmarking
// actually happens (and we don't hit too many false positives).
//
// The code here relies on the fact that |ptr| is in the non-BRP pool and that
// the card table (this object) is allocated at the very beginning of that pool.
class QuarantineCardTable final {
 public:
  // Avoid the load of the base of the non-BRP pool.
  ALWAYS_INLINE static QuarantineCardTable& GetFrom(uintptr_t ptr) {
    PA_SCAN_DCHECK(
        IsManagedByPartitionAllocNonBRPPool(reinterpret_cast<void*>(ptr)));
    constexpr uintptr_t kNonBRPPoolBaseMask =
        PartitionAddressSpace::NonBRPPoolBaseMask();
    return *reinterpret_cast<QuarantineCardTable*>(ptr & kNonBRPPoolBaseMask);
  }

  ALWAYS_INLINE void Quarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, true);
  }

  ALWAYS_INLINE void Unquarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, false);
  }

  // Returns whether the card to which |ptr| points to contains quarantined
  // objects. May return false positives for but should never return false
  // negatives, as otherwise this breaks security.
  ALWAYS_INLINE bool IsQuarantined(uintptr_t ptr) const {
    const size_t byte = Byte(ptr);
    PA_SCAN_DCHECK(byte < bytes_.size());
    return bytes_[byte];
  }

 private:
  static constexpr size_t kCardSize = kPoolMaxSize / kSuperPageSize;
  static constexpr size_t kBytes = kPoolMaxSize / kCardSize;

  QuarantineCardTable() = default;

  ALWAYS_INLINE static constexpr size_t Byte(uintptr_t address) {
    constexpr uintptr_t kNonBRPPoolBaseMask =
        PartitionAddressSpace::NonBRPPoolBaseMask();
    return (address & ~kNonBRPPoolBaseMask) / kCardSize;
  }

  ALWAYS_INLINE void SetImpl(uintptr_t begin, size_t size, bool value) {
    const size_t byte = Byte(begin);
    const size_t need_bytes = (size + (kCardSize - 1)) / kCardSize;
    PA_SCAN_DCHECK(bytes_.size() >= byte + need_bytes);
    PA_SCAN_DCHECK(
        IsManagedByPartitionAllocNonBRPPool(reinterpret_cast<void*>(begin)));
    for (size_t i = byte; i < byte + need_bytes; ++i)
      bytes_[i] = value;
  }

  std::array<bool, kBytes> bytes_;
};
static_assert(kSuperPageSize >= sizeof(QuarantineCardTable),
              "Card table size must be less than kSuperPageSize, since this is "
              "what is committed");
#endif  // PA_STARSCAN_USE_CARD_TABLE

template <typename T>
using MetadataVector = std::vector<T, MetadataAllocator<T>>;
template <typename T>
using MetadataSet = std::set<T, std::less<>, MetadataAllocator<T>>;
template <typename K, typename V>
using MetadataHashMap =
    std::unordered_map<K,
                       V,
                       std::hash<K>,
                       std::equal_to<>,
                       MetadataAllocator<std::pair<const K, V>>>;

// Returns the start of a slot, or nullptr if |maybe_inner_ptr| is not inside of
// an existing slot span. The function may return a non-nullptr pointer even
// inside a decommitted or free slot span, it's the caller responsibility to
// check if memory is actually allocated.
//
// |maybe_inner_ptr| must be within a normal-bucket super page and can also
// point to guard pages or slot-span metadata.
ALWAYS_INLINE uintptr_t GetObjectStartInSuperPage(uintptr_t maybe_inner_ptr,
                                                  const PCScan::Root& root) {
  char* maybe_inner_ptr_as_char_ptr = reinterpret_cast<char*>(maybe_inner_ptr);
  PA_SCAN_DCHECK(IsManagedByNormalBuckets(maybe_inner_ptr_as_char_ptr));
  // Don't use FromSlotInnerPtr() or FromPtr() because they expect a pointer to
  // a valid slot span.
  char* const super_page_ptr =
      reinterpret_cast<char*>(maybe_inner_ptr & kSuperPageBaseMask);

  const uintptr_t partition_page_index =
      (maybe_inner_ptr & kSuperPageOffsetMask) >> PartitionPageShift();
  auto* page = reinterpret_cast<PartitionPage<ThreadSafe>*>(
      PartitionSuperPageToMetadataArea(super_page_ptr) +
      (partition_page_index << kPageMetadataShift));
  // Check if page is valid. The check also works for the guard pages and the
  // metadata page.
  if (!page->is_valid)
    return 0;

  page -= page->slot_span_metadata_offset;
  PA_SCAN_DCHECK(page->is_valid);
  PA_SCAN_DCHECK(!page->slot_span_metadata_offset);
  auto* slot_span = &page->slot_span_metadata;
  // Check if the slot span is actually used and valid.
  if (!slot_span->bucket)
    return 0;
  PA_SCAN_DCHECK(PartitionRoot<ThreadSafe>::IsValidSlotSpan(slot_span));
  char* const slot_span_begin = static_cast<char*>(
      SlotSpanMetadata<ThreadSafe>::ToSlotSpanStartPtr(slot_span));
  const ptrdiff_t ptr_offset = maybe_inner_ptr_as_char_ptr - slot_span_begin;
  PA_SCAN_DCHECK(0 <= ptr_offset &&
                 ptr_offset < static_cast<ptrdiff_t>(
                                  slot_span->bucket->get_pages_per_slot_span() *
                                  PartitionPageSize()));
  // Slot span size in bytes is not necessarily multiple of partition page.
  if (ptr_offset >=
      static_cast<ptrdiff_t>(slot_span->bucket->get_bytes_per_span()))
    return 0;
  const size_t slot_size = slot_span->bucket->slot_size;
  const size_t slot_number = slot_span->bucket->GetSlotNumber(ptr_offset);
  char* const result = slot_span_begin + (slot_number * slot_size);
  PA_SCAN_DCHECK(result <= maybe_inner_ptr_as_char_ptr &&
                 maybe_inner_ptr_as_char_ptr < result + slot_size);
  return reinterpret_cast<uintptr_t>(root.AdjustPointerForExtrasAdd(result));
}

#if PA_SCAN_DCHECK_IS_ON()
bool IsQuarantineEmptyOnSuperPage(uintptr_t super_page) {
  auto* bitmap = SuperPageStateBitmap(reinterpret_cast<char*>(super_page));
  size_t visited = 0;
  bitmap->IterateQuarantined([&visited](auto) { ++visited; });
  return !visited;
}
#endif

SimdSupport DetectSimdSupport() {
#if defined(PA_STARSCAN_NEON_SUPPORTED)
  return SimdSupport::kNEON;
#else
  base::CPU cpu;
  if (cpu.has_avx2())
    return SimdSupport::kAVX2;
  if (cpu.has_sse41())
    return SimdSupport::kSSE41;
  return SimdSupport::kUnvectorized;
#endif  // defined(PA_STARSCAN_NEON_SUPPORTED)
}

void CommitCardTable() {
#if PA_STARSCAN_USE_CARD_TABLE
  RecommitSystemPages(
      reinterpret_cast<void*>(PartitionAddressSpace::NonBRPPoolBase()),
      sizeof(QuarantineCardTable), PageReadWrite, PageUpdatePermissions);
#endif
}

template <class Function>
void IterateNonEmptySlotSpans(uintptr_t super_page_base,
                              size_t nonempty_slot_spans,
                              Function function) {
  PA_SCAN_DCHECK(!(super_page_base % kSuperPageAlignment));
  PA_SCAN_DCHECK(nonempty_slot_spans);

  size_t slot_spans_to_visit = nonempty_slot_spans;
#if PA_SCAN_DCHECK_IS_ON()
  size_t visited = 0;
#endif

  IterateSlotSpans<ThreadSafe>(
      reinterpret_cast<char*>(super_page_base), true /*with_quarantine*/,
      [&function, &slot_spans_to_visit
#if PA_SCAN_DCHECK_IS_ON()
       ,
       &visited
#endif
  ](SlotSpanMetadata<ThreadSafe>* slot_span) {
        if (slot_span->is_empty() || slot_span->is_decommitted()) {
          // Skip empty/decommitted slot spans.
          return false;
        }
        function(slot_span);
        --slot_spans_to_visit;
#if PA_SCAN_DCHECK_IS_ON()
        // In debug builds, scan all the slot spans to check that number of
        // visited slot spans is equal to the number of nonempty_slot_spans.
        ++visited;
        return false;
#else
        return slot_spans_to_visit == 0;
#endif
      });
#if PA_SCAN_DCHECK_IS_ON()
  // Check that exactly all non-empty slot spans have been visited.
  PA_DCHECK(nonempty_slot_spans == visited);
#endif
}

// SuperPageSnapshot is used to record all slot spans that contain live objects.
// The class avoids dynamic allocations and is designed to be instantiated on
// stack. To avoid stack overflow, internal data structures are kept packed.
class SuperPageSnapshot final {
  // The following constants are used to define a conservative estimate for
  // maximum number of slot spans in a super page.
  //
  // For systems with runtime-defined page size, assume partition page size is
  // at least 16kiB.
  static constexpr size_t kMinPartitionPageSize =
      __builtin_constant_p(PartitionPageSize()) ? PartitionPageSize() : 1 << 14;
  static constexpr size_t kStateBitmapMinReservedSize =
      __builtin_constant_p(ReservedStateBitmapSize())
          ? ReservedStateBitmapSize()
          : base::bits::AlignUp(sizeof(AllocationStateMap),
                                kMinPartitionPageSize);
  // Take into account guard partition page at the end of super-page.
  static constexpr size_t kGuardPagesSize = 2 * kMinPartitionPageSize;

  static constexpr size_t kPayloadMaxSize =
      kSuperPageSize - kStateBitmapMinReservedSize - kGuardPagesSize;
  static_assert(kPayloadMaxSize % kMinPartitionPageSize == 0,
                "kPayloadMaxSize must be multiple of kMinPartitionPageSize");

  static constexpr size_t kMaxSlotSpansInSuperPage =
      kPayloadMaxSize / kMinPartitionPageSize;

 public:
  struct ScanArea {
    // Use packed integer types to save stack space. In theory, kAlignment could
    // be used instead of words, but it doesn't seem to bring savings.
    uint32_t offset_within_page_in_words;
    uint32_t size_in_words;
    uint32_t slot_size_in_words;
  };

  class ScanAreas : private std::array<ScanArea, kMaxSlotSpansInSuperPage> {
    using Base = std::array<ScanArea, kMaxSlotSpansInSuperPage>;

   public:
    using iterator = Base::iterator;
    using const_iterator = Base::const_iterator;
    using Base::operator[];

    iterator begin() { return Base::begin(); }
    const_iterator begin() const { return Base::begin(); }

    iterator end() { return std::next(begin(), size_); }
    const_iterator end() const { return std::next(begin(), size_); }

    void set_size(size_t new_size) { size_ = new_size; }

   private:
    size_t size_;
  };

  static_assert(std::is_trivially_default_constructible<ScanAreas>::value,
                "ScanAreas must be trivially default constructible to ensure "
                "that no memsets are generated by the compiler as a "
                "result of value-initialization (or zero-initialization)");

  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;

  // Creates snapshot for a single super page. In theory, we could simply
  // iterate over slot spans without taking a snapshot. However, we do this to
  // minimize the mutex locking time. The mutex must be acquired to make sure
  // that no mutator is concurrently changing any of the slot spans.
  explicit SuperPageSnapshot(uintptr_t super_page_base);

  const ScanAreas& scan_areas() const { return scan_areas_; }

 private:
  ScanAreas scan_areas_;
};

static_assert(
    sizeof(SuperPageSnapshot) <= 2048,
    "SuperPageSnapshot must stay relatively small to be allocated on stack");

SuperPageSnapshot::SuperPageSnapshot(uintptr_t super_page) {
  using Root = PartitionRoot<ThreadSafe>;
  using SlotSpan = SlotSpanMetadata<ThreadSafe>;

  auto* extent_entry = PartitionSuperPageToExtent<ThreadSafe>(
      reinterpret_cast<char*>(super_page));

  typename Root::ScopedGuard lock(extent_entry->root->lock_);

  const size_t nonempty_slot_spans =
      extent_entry->number_of_nonempty_slot_spans;
  if (!nonempty_slot_spans) {
#if PA_SCAN_DCHECK_IS_ON()
    // Check that quarantine bitmap is empty for super-pages that contain
    // only empty/decommitted slot-spans.
    PA_CHECK(IsQuarantineEmptyOnSuperPage(super_page));
#endif
    scan_areas_.set_size(0);
    return;
  }

  size_t current = 0;

  IterateNonEmptySlotSpans(
      super_page, nonempty_slot_spans, [this, &current](SlotSpan* slot_span) {
        const uintptr_t payload_begin = reinterpret_cast<uintptr_t>(
            SlotSpan::ToSlotSpanStartPtr(slot_span));
        // For single-slot slot-spans, scan only utilized slot part.
        const size_t provisioned_size = UNLIKELY(slot_span->CanStoreRawSize())
                                            ? slot_span->GetRawSize()
                                            : slot_span->GetProvisionedSize();
        // Free & decommitted slot spans are skipped.
        PA_SCAN_DCHECK(provisioned_size > 0);
        const uintptr_t payload_end = payload_begin + provisioned_size;
        auto& area = scan_areas_[current];

        const size_t offset_in_words =
            (payload_begin & kSuperPageOffsetMask) / sizeof(uintptr_t);
        const size_t size_in_words =
            (payload_end - payload_begin) / sizeof(uintptr_t);
        const size_t slot_size_in_words =
            slot_span->bucket->slot_size / sizeof(uintptr_t);

#if PA_SCAN_DCHECK_IS_ON()
        PA_DCHECK(offset_in_words <=
                  std::numeric_limits<decltype(
                      area.offset_within_page_in_words)>::max());
        PA_DCHECK(size_in_words <=
                  std::numeric_limits<decltype(area.size_in_words)>::max());
        PA_DCHECK(
            slot_size_in_words <=
            std::numeric_limits<decltype(area.slot_size_in_words)>::max());
#endif

        area.offset_within_page_in_words = offset_in_words;
        area.size_in_words = size_in_words;
        area.slot_size_in_words = slot_size_in_words;

        ++current;
      });

  PA_SCAN_DCHECK(kMaxSlotSpansInSuperPage >= current);
  scan_areas_.set_size(current);
}

}  // namespace

class PCScanScanLoop;

// This class is responsible for performing the entire PCScan task.
// TODO(bikineev): Move PCScan algorithm out of PCScanTask.
class PCScanTask final : public base::RefCountedThreadSafe<PCScanTask>,
                         public AllocatedOnPCScanMetadataPartition {
 public:
  // Creates and initializes a PCScan state.
  PCScanTask(PCScan& pcscan, size_t quarantine_last_size);

  PCScanTask(PCScanTask&&) noexcept = delete;
  PCScanTask& operator=(PCScanTask&&) noexcept = delete;

  // Execute PCScan from mutator inside safepoint.
  void RunFromMutator();

  // Execute PCScan from the scanner thread. Must be called only once from the
  // scanner thread.
  void RunFromScanner();

  PCScanScheduler& scheduler() const { return pcscan_.scheduler(); }

 private:
  class StackVisitor;
  friend class PCScanScanLoop;

  using Root = PCScan::Root;
  using SlotSpan = SlotSpanMetadata<ThreadSafe>;

  // This is used:
  // - to synchronize all scanning threads (mutators and the scanner);
  // - for the scanner, to transition through the state machine
  //   (kScheduled -> kScanning (ctor) -> kSweepingAndFinishing (dtor).
  template <Context context>
  class SyncScope final {
   public:
    explicit SyncScope(PCScanTask& task) : task_(task) {
      task_.number_of_scanning_threads_.fetch_add(1, std::memory_order_relaxed);
      if (context == Context::kScanner) {
        task_.pcscan_.state_.store(PCScan::State::kScanning,
                                   std::memory_order_relaxed);
        task_.pcscan_.SetJoinableIfSafepointEnabled(true);
      }
    }
    ~SyncScope() {
      // First, notify the scanning thread that this thread is done.
      NotifyThreads();
      if (context == Context::kScanner) {
        // The scanner thread must wait here until all safepoints leave.
        // Otherwise, sweeping may free a page that can later be accessed by a
        // descheduled mutator.
        WaitForOtherThreads();
        task_.pcscan_.state_.store(PCScan::State::kSweepingAndFinishing,
                                   std::memory_order_relaxed);
      }
    }

   private:
    void NotifyThreads() {
      {
        // The lock is required as otherwise there is a race between
        // fetch_sub/notify in the mutator and checking
        // number_of_scanning_threads_/waiting in the scanner.
        std::lock_guard<std::mutex> lock(task_.mutex_);
        task_.number_of_scanning_threads_.fetch_sub(1,
                                                    std::memory_order_relaxed);
        {
          // Notify that scan is done and there is no need to enter
          // the safepoint. This also helps a mutator to avoid repeating
          // entering. Since the scanner thread waits for all threads to finish,
          // there is no ABA problem here.
          task_.pcscan_.SetJoinableIfSafepointEnabled(false);
        }
      }
      task_.condvar_.notify_all();
    }

    void WaitForOtherThreads() {
      std::unique_lock<std::mutex> lock(task_.mutex_);
      task_.condvar_.wait(lock, [this] {
        return !task_.number_of_scanning_threads_.load(
            std::memory_order_relaxed);
      });
    }

    PCScanTask& task_;
  };

  friend class base::RefCountedThreadSafe<PCScanTask>;
  ~PCScanTask() = default;

  ALWAYS_INLINE AllocationStateMap* TryFindScannerBitmapForPointer(
      uintptr_t maybe_ptr) const;

  // Lookup and marking functions. Return size of the object if marked or zero
  // otherwise.
  ALWAYS_INLINE size_t TryMarkObjectInNormalBuckets(uintptr_t maybe_ptr) const;

  // Scans stack, only called from safepoints.
  void ScanStack();

  // Scan individual areas.
  void ScanNormalArea(PCScanInternal& pcscan,
                      PCScanScanLoop& scan_loop,
                      uintptr_t* begin,
                      uintptr_t* end);
  void ScanLargeArea(PCScanInternal& pcscan,
                     PCScanScanLoop& scan_loop,
                     uintptr_t* begin,
                     uintptr_t* end,
                     size_t slot_size);

  // Scans all registered partitions and marks reachable quarantined objects.
  void ScanPartitions();

  // Clear quarantined objects and prepare card table for fast lookup
  void ClearQuarantinedObjectsAndPrepareCardTable();

  // Unprotect all slot spans from all partitions.
  void UnprotectPartitions();

  // Sweeps (frees) unreachable quarantined entries. Returns the size of swept
  // objects.
  void SweepQuarantine();

  // Finishes the scanner (updates limits, UMA, etc).
  void FinishScanner();

  // Cache the pcscan epoch to avoid the compiler loading the atomic
  // QuarantineData::epoch_ on each access.
  const size_t pcscan_epoch_;
  std::unique_ptr<StarScanSnapshot> snapshot_;
  StatsCollector stats_;
  // Mutex and codvar that are used to synchronize scanning threads.
  std::mutex mutex_;
  std::condition_variable condvar_;
  std::atomic<size_t> number_of_scanning_threads_{0u};
  // We can unprotect only once to reduce context-switches.
  std::once_flag unprotect_once_flag_;
  bool immediatelly_free_objects_{false};
  PCScan& pcscan_;
};

ALWAYS_INLINE AllocationStateMap* PCScanTask::TryFindScannerBitmapForPointer(
    uintptr_t maybe_ptr) const {
  PA_SCAN_DCHECK(
      IsManagedByPartitionAllocNonBRPPool(reinterpret_cast<void*>(maybe_ptr)));
  // First, check if |maybe_ptr| points to a valid super page or a quarantined
  // card.
#if defined(PA_HAS_64_BITS_POINTERS)
#if PA_STARSCAN_USE_CARD_TABLE
  // Check if |maybe_ptr| points to a quarantined card.
  if (LIKELY(!QuarantineCardTable::GetFrom(maybe_ptr).IsQuarantined(maybe_ptr)))
    return nullptr;
#else
  // Without the card table, use the reservation offset table to check if
  // |maybe_ptr| points to a valid super-page. It's not as precise (meaning that
  // we may have hit the slow path more frequently), but reduces the memory
  // overhead.  Since we are certain here, that |maybe_ptr| refers to the
  // non-BRP pool, it's okay to use non-checking version of
  // ReservationOffsetPointer().
  const uintptr_t offset =
      maybe_ptr & ~PartitionAddressSpace::NonBRPPoolBaseMask();
  if (LIKELY(*ReservationOffsetPointer(kNonBRPPoolHandle, offset) !=
             kOffsetTagNormalBuckets))
    return nullptr;
#endif
#else   // defined(PA_HAS_64_BITS_POINTERS)
  if (LIKELY(!IsManagedByPartitionAllocNonBRPPool(
          reinterpret_cast<void*>(maybe_ptr))))
    return nullptr;
#endif  // defined(PA_HAS_64_BITS_POINTERS)

  // We are certain here that |maybe_ptr| points to an allocated super-page.
  return StateBitmapFromPointer(reinterpret_cast<char*>(maybe_ptr));
}

// Looks up and marks a potential dangling pointer. Returns the size of the slot
// (which is then accounted as quarantined) or zero if no object is found.
// For normal bucket super pages, PCScan uses two quarantine bitmaps, the
// mutator and the scanner one. The former is used by mutators when objects are
// freed, while the latter is used concurrently by the PCScan thread. The
// bitmaps are swapped as soon as PCScan is triggered. Once a dangling pointer
// (which points to an object in the scanner bitmap) is found,
// TryMarkObjectInNormalBuckets() marks it again in the bitmap and clears
// from the scanner bitmap. This way, when scanning is done, all uncleared
// entries in the scanner bitmap correspond to unreachable objects.
ALWAYS_INLINE size_t
PCScanTask::TryMarkObjectInNormalBuckets(uintptr_t maybe_ptr) const {
  // Check if |maybe_ptr| points somewhere to the heap.
  auto* state_map = TryFindScannerBitmapForPointer(maybe_ptr);
  if (!state_map)
    return 0;

  // Beyond this point, we know that |maybe_ptr| is a pointer within a
  // normal-bucket super page.
  PA_SCAN_DCHECK(IsManagedByNormalBuckets(reinterpret_cast<void*>(maybe_ptr)));
  // Pointer from a normal bucket is always in the first superpage.
  auto* root =
      Root::FromPointerInFirstSuperpage(reinterpret_cast<char*>(maybe_ptr));

#if !PA_STARSCAN_USE_CARD_TABLE
  // Without the card table, we must make sure that |maybe_ptr| doesn't point to
  // metadata partition.
  // TODO(bikineev): To speed things up, consider removing the check and
  // committing quarantine bitmaps for metadata partition.
  if (UNLIKELY(!root->IsQuarantineEnabled()))
    return 0;
#endif

  // Check if pointer was in the quarantine bitmap.
  const uintptr_t base = GetObjectStartInSuperPage(maybe_ptr, *root);
  if (!base || !state_map->IsQuarantined(base))
    return 0;

  PA_SCAN_DCHECK((maybe_ptr & kSuperPageBaseMask) ==
                 (base & kSuperPageBaseMask));

  auto* target_slot_span =
      SlotSpan::FromSlotInnerPtr(reinterpret_cast<void*>(base));
  PA_SCAN_DCHECK(root == Root::FromSlotSpan(target_slot_span));

  const size_t usable_size = target_slot_span->GetUsableSize(root);
  // Range check for inner pointers.
  if (maybe_ptr >= base + usable_size)
    return 0;

  if (UNLIKELY(immediatelly_free_objects_))
    return 0;

  // Now we are certain that |maybe_ptr| is a dangling pointer. Mark it again in
  // the mutator bitmap and clear from the scanner bitmap. Note that since
  // PCScan has exclusive access to the scanner bitmap, we can avoid atomic rmw
  // operation for it.
  if (LIKELY(state_map->MarkQuarantinedAsReachable(base, pcscan_epoch_)))
    return target_slot_span->bucket->slot_size;

  return 0;
}

void PCScanTask::ClearQuarantinedObjectsAndPrepareCardTable() {
  const PCScan::ClearType clear_type = pcscan_.clear_type_;

#if !PA_STARSCAN_USE_CARD_TABLE
  if (clear_type == PCScan::ClearType::kEager)
    return;
#endif

  StarScanSnapshot::ClearingView view(*snapshot_);
  view.VisitConcurrently([clear_type](uintptr_t super_page_base) {
    auto* bitmap =
        StateBitmapFromPointer(reinterpret_cast<char*>(super_page_base));
    auto* root = Root::FromSuperPage(reinterpret_cast<char*>(super_page_base));
    bitmap->IterateQuarantined([root, clear_type](uintptr_t ptr) {
      auto* object = reinterpret_cast<void*>(ptr);
      auto* slot_span = SlotSpan::FromSlotInnerPtr(object);
      // Use zero as a zapping value to speed up the fast bailout check in
      // ScanPartitions.
      const size_t size = slot_span->GetUsableSize(root);
      if (clear_type == PCScan::ClearType::kLazy)
        memset(object, 0, size);
#if PA_STARSCAN_USE_CARD_TABLE
      // Set card(s) for this quarantined object.
      QuarantineCardTable::GetFrom(ptr).Quarantine(ptr, size);
#endif
    });
  });
}

void PCScanTask::UnprotectPartitions() {
  auto& pcscan = PCScanInternal::Instance();
  if (!pcscan.WriteProtectionEnabled())
    return;

  StarScanSnapshot::UnprotectingView unprotect_view(*snapshot_);
  unprotect_view.VisitConcurrently([&pcscan](uintptr_t super_page) {
    SuperPageSnapshot super_page_snapshot(super_page);

    for (const auto& scan_area : super_page_snapshot.scan_areas()) {
      const uintptr_t begin =
          super_page |
          (scan_area.offset_within_page_in_words * sizeof(uintptr_t));
      const uintptr_t end =
          begin + (scan_area.size_in_words * sizeof(uintptr_t));

      pcscan.UnprotectPages(begin, end - begin);
    }
  });
}

class PCScanScanLoop final : public ScanLoop<PCScanScanLoop> {
  friend class ScanLoop<PCScanScanLoop>;

 public:
  explicit PCScanScanLoop(const PCScanTask& task)
      : ScanLoop(PCScanInternal::Instance().simd_support()),
#if defined(PA_HAS_64_BITS_POINTERS)
        giga_cage_base_(PartitionAddressSpace::NonBRPPoolBase()),
#endif
        task_(task) {
  }

  size_t quarantine_size() const { return quarantine_size_; }

 private:
  ALWAYS_INLINE uintptr_t CageBase() const { return giga_cage_base_; }
  ALWAYS_INLINE static constexpr uintptr_t CageMask() {
#if defined(PA_HAS_64_BITS_POINTERS)
    return PartitionAddressSpace::NonBRPPoolBaseMask();
#else
    return 0;
#endif
  }

  ALWAYS_INLINE void CheckPointer(uintptr_t maybe_ptr) {
    quarantine_size_ += task_.TryMarkObjectInNormalBuckets(maybe_ptr);
  }

  const uintptr_t giga_cage_base_ = 0;
  const PCScanTask& task_;
  size_t quarantine_size_ = 0;
};

class PCScanTask::StackVisitor final : public internal::StackVisitor {
 public:
  explicit StackVisitor(const PCScanTask& task) : task_(task) {}

  void VisitStack(uintptr_t* stack_ptr, uintptr_t* stack_top) override {
    static constexpr size_t kMinimalAlignment = 32;
    stack_ptr = reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(stack_ptr) & ~(kMinimalAlignment - 1));
    stack_top = reinterpret_cast<uintptr_t*>(
        (reinterpret_cast<uintptr_t>(stack_top) + kMinimalAlignment - 1) &
        ~(kMinimalAlignment - 1));
    PA_CHECK(stack_ptr < stack_top);
    PCScanScanLoop loop(task_);
    loop.Run(stack_ptr, stack_top);
    quarantine_size_ += loop.quarantine_size();
  }

  // Returns size of quarantined objects that are reachable from the current
  // stack.
  size_t quarantine_size() const { return quarantine_size_; }

 private:
  const PCScanTask& task_;
  size_t quarantine_size_ = 0;
};

PCScanTask::PCScanTask(PCScan& pcscan, size_t quarantine_last_size)
    : pcscan_epoch_(pcscan.epoch() - 1),
      snapshot_(StarScanSnapshot::Create(PCScanInternal::Instance())),
      stats_(PCScanInternal::Instance().process_name(), quarantine_last_size),
      immediatelly_free_objects_(
          PCScanInternal::Instance().IsImmediateFreeingEnabled()),
      pcscan_(pcscan) {}

void PCScanTask::ScanStack() {
  const auto& pcscan = PCScanInternal::Instance();
  if (!pcscan.IsStackScanningEnabled())
    return;
  // Check if the stack top was registered. It may happen that it's not if the
  // current allocation happens from pthread trampolines.
  void* stack_top = pcscan.GetCurrentThreadStackTop();
  if (UNLIKELY(!stack_top))
    return;

  Stack stack_scanner(stack_top);
  StackVisitor visitor(*this);
  stack_scanner.IteratePointers(&visitor);
  stats_.IncreaseSurvivedQuarantineSize(visitor.quarantine_size());
}

void PCScanTask::ScanNormalArea(PCScanInternal& pcscan,
                                PCScanScanLoop& scan_loop,
                                uintptr_t* begin,
                                uintptr_t* end) {
  // Protect slot span before scanning it.
  pcscan.ProtectPages(reinterpret_cast<uintptr_t>(begin),
                      (end - begin) * sizeof(uintptr_t));
  scan_loop.Run(begin, end);
}

void PCScanTask::ScanLargeArea(PCScanInternal& pcscan,
                               PCScanScanLoop& scan_loop,
                               uintptr_t* begin,
                               uintptr_t* end,
                               size_t slot_size) {
  // For scanning large areas, it's worthwhile checking whether the range that
  // is scanned contains allocated objects. It also helps to skip discarded
  // freed slots.
  // Protect slot span before scanning it.
  pcscan.ProtectPages(reinterpret_cast<uintptr_t>(begin),
                      (end - begin) * sizeof(uintptr_t));

  auto* bitmap = StateBitmapFromPointer(reinterpret_cast<void*>(begin));
  const size_t slot_size_in_words = slot_size / sizeof(uintptr_t);

  for (uintptr_t* current_slot = begin; current_slot < end;
       current_slot += slot_size_in_words) {
    // It is okay to skip objects as their payload has been zapped at this
    // point which means that the pointers no longer retain other objects.
    if (!bitmap->IsAllocated(reinterpret_cast<uintptr_t>(current_slot))) {
      continue;
    }
    uintptr_t* current_slot_end = current_slot + slot_size_in_words;
    // |slot_size| may be larger than |raw_size| for single-slot slot spans.
    scan_loop.Run(current_slot, std::min(current_slot_end, end));
  }
}

void PCScanTask::ScanPartitions() {
  // Threshold for which bucket size it is worthwhile in checking whether the
  // object is allocated and needs to be scanned. PartitionPurgeSlotSpan()
  // purges only slots >= page-size, this helps us to avoid faulting in
  // discarded pages. We actually lower it further to 1024, to take advantage of
  // skipping unallocated slots, but don't want to go any lower, as this comes
  // at a cost of expensive bitmap checking.
  static constexpr size_t kLargeScanAreaThresholdInWords =
      1024 / sizeof(uintptr_t);

  PCScanScanLoop scan_loop(*this);
  auto& pcscan = PCScanInternal::Instance();

  StarScanSnapshot::ScanningView snapshot_view(*snapshot_);
  snapshot_view.VisitConcurrently(
      [this, &pcscan, &scan_loop](uintptr_t super_page) {
        SuperPageSnapshot super_page_snapshot(super_page);

        for (const auto& scan_area : super_page_snapshot.scan_areas()) {
          auto* const begin = reinterpret_cast<uintptr_t*>(
              super_page |
              (scan_area.offset_within_page_in_words * sizeof(uintptr_t)));
          auto* const end = begin + scan_area.size_in_words;

          if (UNLIKELY(scan_area.slot_size_in_words >=
                       kLargeScanAreaThresholdInWords)) {
            ScanLargeArea(pcscan, scan_loop, begin, end,
                          scan_area.slot_size_in_words * sizeof(uintptr_t));
          } else {
            ScanNormalArea(pcscan, scan_loop, begin, end);
          }
        }
      });

  stats_.IncreaseSurvivedQuarantineSize(scan_loop.quarantine_size());
}

namespace {

size_t FreeAndUnmarkInCardTable(PartitionRoot<ThreadSafe>* root,
                                SlotSpanMetadata<ThreadSafe>* slot_span,
                                void* object) {
  const size_t slot_size = slot_span->bucket->slot_size;
  root->FreeNoHooksImmediate(object, slot_span);
#if PA_STARSCAN_USE_CARD_TABLE
  // Reset card(s) for this quarantined object. Please note that the
  // cards may still contain quarantined objects (which were
  // promoted in this scan cycle), but
  // ClearQuarantinedObjectsAndFilterSuperPages() will set them
  // again in the next PCScan cycle.
  QuarantineCardTable::GetFrom(ptr).Unquarantine(
      ptr, slot_span->GetUsableSize(root));
#endif
  return slot_size;
}

struct SweepStat {
  // Bytes that were really swept (by calling free()).
  size_t swept_bytes = 0;
  // Bytes of marked quarantine memory that were discarded (by calling
  // madvice(DONT_NEED)).
  size_t discarded_bytes = 0;
};

void SweepSuperPage(ThreadSafePartitionRoot* root,
                    void* super_page,
                    size_t epoch,
                    SweepStat& stat) {
  auto* bitmap = StateBitmapFromPointer(super_page);
  ThreadSafePartitionRoot::FromSuperPage(static_cast<char*>(super_page));
  bitmap->IterateUnmarkedQuarantined(epoch, [root, &stat](uintptr_t ptr) {
    auto* object = reinterpret_cast<void*>(ptr);
    auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotInnerPtr(object);
    stat.swept_bytes +=
        FreeAndUnmarkInCardTable(root, slot_span, reinterpret_cast<void*>(ptr));
  });
}

void SweepSuperPageAndDiscardMarkedQuarantine(ThreadSafePartitionRoot* root,
                                              void* super_page,
                                              size_t epoch,
                                              SweepStat& stat) {
  auto* bitmap = StateBitmapFromPointer(super_page);
  bitmap->IterateQuarantined(epoch, [root, &stat](uintptr_t ptr,
                                                  bool is_marked) {
    auto* object = reinterpret_cast<void*>(ptr);
    auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotInnerPtr(object);
    if (LIKELY(!is_marked)) {
      stat.swept_bytes += FreeAndUnmarkInCardTable(root, slot_span, object);
      return;
    }
    // Otherwise, try to discard pages for marked quarantine. Since no data is
    // stored in quarantined objects (e.g. the |next| pointer), this can be
    // freely done.
    const size_t slot_size = slot_span->bucket->slot_size;
    if (slot_size >= SystemPageSize()) {
      // Since no data is stored in quarantined objects (e.g. the |next|
      // pointer), we can freely discard physical memory.
      const uintptr_t discard_end =
          bits::AlignDown(ptr + slot_size, SystemPageSize());
      const uintptr_t discard_begin = bits::AlignUp(ptr, SystemPageSize());
      const intptr_t discard_size = discard_end - discard_begin;
      if (discard_size > 0) {
        DiscardSystemPages(reinterpret_cast<void*>(discard_begin),
                           discard_size);
        stat.discarded_bytes += discard_size;
      }
    }
  });
}

}  // namespace

void PCScanTask::SweepQuarantine() {
  // Check that scan is unjoinable by this time.
  PA_DCHECK(!pcscan_.IsJoinable());
  // Discard marked quarantine memory on every Nth scan.
  // TODO(bikineev): Find a better signal (e.g. memory pressure, high
  // survival rate, etc).
  static constexpr size_t kDiscardMarkedQuarantineFrequency = 16;
  const bool should_discard =
      (pcscan_epoch_ % kDiscardMarkedQuarantineFrequency == 0) &&
      (pcscan_.clear_type_ == PCScan::ClearType::kEager);

  SweepStat stat;
  StarScanSnapshot::SweepingView sweeping_view(*snapshot_);
  sweeping_view.VisitNonConcurrently(
      [this, &stat, should_discard](uintptr_t super_page) {
        void* super_page_as_void = reinterpret_cast<void*>(super_page);
        auto* root = ThreadSafePartitionRoot::FromSuperPage(
            static_cast<char*>(super_page_as_void));

        if (UNLIKELY(should_discard && !root->allow_cookie))
          SweepSuperPageAndDiscardMarkedQuarantine(root, super_page_as_void,
                                                   pcscan_epoch_, stat);
        else
          SweepSuperPage(root, super_page_as_void, pcscan_epoch_, stat);
      });

  stats_.IncreaseSweptSize(stat.swept_bytes);
  stats_.IncreaseDiscardedQuarantineSize(stat.discarded_bytes);

#if defined(PA_THREAD_CACHE_SUPPORTED)
  // Sweeping potentially frees into the current thread's thread cache. Purge
  // releases the cache back to the global allocator.
  auto* current_thread_tcache = ThreadCache::Get();
  if (ThreadCache::IsValid(current_thread_tcache))
    current_thread_tcache->Purge();
#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

void PCScanTask::FinishScanner() {
  stats_.ReportTracesAndHists();

  pcscan_.scheduler_.scheduling_backend().UpdateScheduleAfterScan(
      stats_.survived_quarantine_size(), stats_.GetOverallTime(),
      PCScanInternal::Instance().CalculateTotalHeapSize());

  PCScanInternal::Instance().ResetCurrentPCScanTask();
  // Change the state and check that concurrent task can't be scheduled twice.
  PA_CHECK(pcscan_.state_.exchange(PCScan::State::kNotRunning,
                                   std::memory_order_acq_rel) ==
           PCScan::State::kSweepingAndFinishing);
}

void PCScanTask::RunFromMutator() {
  ReentrantScannerGuard reentrancy_guard;
  StatsCollector::MutatorScope overall_scope(
      stats_, StatsCollector::MutatorId::kOverall);
  {
    SyncScope<Context::kMutator> sync_scope(*this);
    // Mutator might start entering the safepoint while scanning was already
    // finished.
    if (!pcscan_.IsJoinable())
      return;
    {
      // Clear all quarantined objects and prepare card table.
      StatsCollector::MutatorScope clear_scope(
          stats_, StatsCollector::MutatorId::kClear);
      ClearQuarantinedObjectsAndPrepareCardTable();
    }
    {
      // Scan the thread's stack to find dangling references.
      StatsCollector::MutatorScope scan_scope(
          stats_, StatsCollector::MutatorId::kScanStack);
      ScanStack();
    }
    {
      // Unprotect all scanned pages, if needed.
      UnprotectPartitions();
    }
    {
      // Scan heap for dangling references.
      StatsCollector::MutatorScope scan_scope(stats_,
                                              StatsCollector::MutatorId::kScan);
      ScanPartitions();
    }
  }
}

void PCScanTask::RunFromScanner() {
  ReentrantScannerGuard reentrancy_guard;
  {
    StatsCollector::ScannerScope overall_scope(
        stats_, StatsCollector::ScannerId::kOverall);
    {
      SyncScope<Context::kScanner> sync_scope(*this);
      {
        // Clear all quarantined objects and prepare the card table.
        StatsCollector::ScannerScope clear_scope(
            stats_, StatsCollector::ScannerId::kClear);
        ClearQuarantinedObjectsAndPrepareCardTable();
      }
      {
        // Scan heap for dangling references.
        StatsCollector::ScannerScope scan_scope(
            stats_, StatsCollector::ScannerId::kScan);
        ScanPartitions();
      }
      {
        // Unprotect all scanned pages, if needed.
        UnprotectPartitions();
      }
    }
    {
      // Sweep unreachable quarantined objects.
      StatsCollector::ScannerScope sweep_scope(
          stats_, StatsCollector::ScannerId::kSweep);
      SweepQuarantine();
    }
  }
  FinishScanner();
}

class PCScan::PCScanThread final {
 public:
  using TaskHandle = PCScanInternal::TaskHandle;

  static PCScanThread& Instance() {
    // Lazily instantiate the scanning thread.
    static base::NoDestructor<PCScanThread> instance;
    return *instance;
  }

  void PostTask(TaskHandle task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      PA_DCHECK(!posted_task_.get());
      posted_task_ = std::move(task);
      wanted_delay_ = TimeDelta();
    }
    condvar_.notify_one();
  }

  void PostDelayedTask(TimeDelta delay) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (posted_task_.get()) {
        return;
      }
      wanted_delay_ = delay;
    }
    condvar_.notify_one();
  }

 private:
  friend class base::NoDestructor<PCScanThread>;

  PCScanThread() {
    std::thread{[this] {
      static constexpr const char* kThreadName = "PCScan";
      // Ideally we should avoid mixing base:: and std:: API for threading, but
      // this is useful for visualizing the pcscan thread in chrome://tracing.
      base::PlatformThread::SetName(kThreadName);
      TaskLoop();
    }}.detach();
  }

  // Waits and returns whether the delay should be recomputed.
  bool Wait(std::unique_lock<std::mutex>& lock) {
    PA_DCHECK(lock.owns_lock());
    if (wanted_delay_.is_zero()) {
      condvar_.wait(lock, [this] {
        // Re-evaluate if either delay changed, or a task was
        // enqueued.
        return !wanted_delay_.is_zero() || posted_task_.get();
      });
      // The delay has already been set up and should not be queried again.
      return false;
    }
    condvar_.wait_for(
        lock, std::chrono::microseconds(wanted_delay_.InMicroseconds()));
    // If no task has been posted, the delay should be recomputed at this point.
    return !posted_task_.get();
  }

  void TaskLoop() {
    while (true) {
      TaskHandle current_task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        // Scheduling.
        while (!posted_task_.get()) {
          if (Wait(lock)) {
            wanted_delay_ =
                scheduler().scheduling_backend().UpdateDelayedSchedule();
            if (wanted_delay_.is_zero()) {
              break;
            }
          }
        }
        // Differentiate between a posted task and a delayed task schedule.
        if (posted_task_.get()) {
          std::swap(current_task, posted_task_);
          wanted_delay_ = TimeDelta();
        } else {
          PA_DCHECK(wanted_delay_.is_zero());
        }
      }
      // Differentiate between a posted task and a delayed task schedule.
      if (current_task.get()) {
        current_task->RunFromScanner();
      } else {
        PCScan::Instance().PerformScan(PCScan::InvocationMode::kNonBlocking);
      }
    }
  }

  PCScanScheduler& scheduler() const { return PCScan::Instance().scheduler(); }

  std::mutex mutex_;
  std::condition_variable condvar_;
  TaskHandle posted_task_;
  TimeDelta wanted_delay_;
};

PCScanInternal::PCScanInternal() : simd_support_(DetectSimdSupport()) {}

PCScanInternal::~PCScanInternal() = default;

void PCScanInternal::Initialize(PCScan::InitConfig config) {
  PA_DCHECK(!is_initialized_);
#if defined(PA_HAS_64_BITS_POINTERS)
  // Make sure that GigaCage is initialized.
  PartitionAddressSpace::Init();
#endif
  CommitCardTable();
#if defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
  if (config.write_protection ==
      PCScan::InitConfig::WantedWriteProtectionMode::kEnabled)
    write_protector_ = std::make_unique<UserFaultFDWriteProtector>();
  else
    write_protector_ = std::make_unique<NoWriteProtector>();
#else
  write_protector_ = std::make_unique<NoWriteProtector>();
#endif  // defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
  PCScan::SetClearType(write_protector_->SupportedClearType());

  if (config.safepoint == PCScan::InitConfig::SafepointMode::kEnabled) {
    PCScan::Instance().EnableSafepoints();
  }
  scannable_roots_ = RootsMap();
  nonscannable_roots_ = RootsMap();
  is_initialized_ = true;
}

void PCScanInternal::PerformScan(PCScan::InvocationMode invocation_mode) {
#if PA_SCAN_DCHECK_IS_ON()
  PA_DCHECK(is_initialized());
  PA_DCHECK(scannable_roots().size() > 0);
  PA_DCHECK(std::all_of(
      scannable_roots().begin(), scannable_roots().end(),
      [](const auto& pair) { return pair.first->IsScanEnabled(); }));
  PA_DCHECK(std::all_of(
      nonscannable_roots().begin(), nonscannable_roots().end(),
      [](const auto& pair) { return pair.first->IsQuarantineEnabled(); }));
#endif

  PCScan& frontend = PCScan::Instance();
  {
    // If scanning is already in progress, bail out.
    PCScan::State expected = PCScan::State::kNotRunning;
    if (!frontend.state_.compare_exchange_strong(
            expected, PCScan::State::kScheduled, std::memory_order_acq_rel,
            std::memory_order_relaxed))
      return;
  }

  const size_t last_quarantine_size =
      frontend.scheduler_.scheduling_backend().ScanStarted();

  // Create PCScan task and set it as current.
  auto task = base::MakeRefCounted<PCScanTask>(frontend, last_quarantine_size);
  PCScanInternal::Instance().SetCurrentPCScanTask(task);

  if (UNLIKELY(invocation_mode ==
               PCScan::InvocationMode::kScheduleOnlyForTesting)) {
    // Immediately change the state to enable safepoint testing.
    frontend.state_.store(PCScan::State::kScanning, std::memory_order_release);
    frontend.SetJoinableIfSafepointEnabled(true);
    return;
  }

  // Post PCScan task.
  if (LIKELY(invocation_mode == PCScan::InvocationMode::kNonBlocking)) {
    PCScan::PCScanThread::Instance().PostTask(std::move(task));
  } else {
    PA_SCAN_DCHECK(PCScan::InvocationMode::kBlocking == invocation_mode ||
                   PCScan::InvocationMode::kForcedBlocking == invocation_mode);
    std::move(*task).RunFromScanner();
  }
}

void PCScanInternal::PerformScanIfNeeded(
    PCScan::InvocationMode invocation_mode) {
  if (!scannable_roots().size())
    return;
  PCScan& frontend = PCScan::Instance();
  if (invocation_mode == PCScan::InvocationMode::kForcedBlocking ||
      frontend.scheduler_.scheduling_backend()
          .GetQuarantineData()
          .MinimumScanningThresholdReached())
    PerformScan(invocation_mode);
}

void PCScanInternal::PerformDelayedScan(TimeDelta delay) {
  PCScan::PCScanThread::Instance().PostDelayedTask(delay);
}

void PCScanInternal::JoinScan() {
  // Current task can be destroyed by the scanner. Check that it's valid.
  if (auto current_task = CurrentPCScanTask())
    current_task->RunFromMutator();
}

PCScanInternal::TaskHandle PCScanInternal::CurrentPCScanTask() const {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  return current_task_;
}

void PCScanInternal::SetCurrentPCScanTask(TaskHandle task) {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  current_task_ = std::move(task);
}

void PCScanInternal::ResetCurrentPCScanTask() {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  current_task_.reset();
}

namespace {
PCScanInternal::SuperPages GetSuperPagesAndCommitStateBitmaps(
    PCScan::Root& root) {
  const size_t state_bitmap_size_to_commit = CommittedStateBitmapSize();
  PCScanInternal::SuperPages super_pages;
  for (auto* super_page_extent = root.first_extent; super_page_extent;
       super_page_extent = super_page_extent->next) {
    for (char *super_page = SuperPagesBeginFromExtent(super_page_extent),
              *super_page_end = SuperPagesEndFromExtent(super_page_extent);
         super_page != super_page_end; super_page += kSuperPageSize) {
      RecommitSystemPages(internal::SuperPageStateBitmap(super_page),
                          state_bitmap_size_to_commit, PageReadWrite,
                          PageUpdatePermissions);
      super_pages.push_back(reinterpret_cast<uintptr_t>(super_page));
    }
  }
  return super_pages;
}
}  // namespace

void PCScanInternal::RegisterScannableRoot(Root* root) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  // Avoid nesting locks and store super_pages in a temporary vector.
  SuperPages super_pages;
  {
    typename Root::ScopedGuard guard(root->lock_);
    PA_CHECK(root->IsQuarantineAllowed());
    if (root->IsScanEnabled())
      return;
    PA_CHECK(!root->IsQuarantineEnabled());
    super_pages = GetSuperPagesAndCommitStateBitmaps(*root);
    root->scan_mode = Root::ScanMode::kEnabled;
    root->quarantine_mode = Root::QuarantineMode::kEnabled;
  }
  std::lock_guard<std::mutex> lock(roots_mutex_);
  PA_DCHECK(!scannable_roots_.count(root));
  auto& root_super_pages = scannable_roots_[root];
  root_super_pages.insert(root_super_pages.end(), super_pages.begin(),
                          super_pages.end());
}

void PCScanInternal::RegisterNonScannableRoot(Root* root) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  // Avoid nesting locks and store super_pages in a temporary vector.
  SuperPages super_pages;
  {
    typename Root::ScopedGuard guard(root->lock_);
    PA_CHECK(root->IsQuarantineAllowed());
    PA_CHECK(!root->IsScanEnabled());
    if (root->IsQuarantineEnabled())
      return;
    super_pages = GetSuperPagesAndCommitStateBitmaps(*root);
    root->quarantine_mode = Root::QuarantineMode::kEnabled;
  }
  std::lock_guard<std::mutex> lock(roots_mutex_);
  PA_DCHECK(!nonscannable_roots_.count(root));
  auto& root_super_pages = nonscannable_roots_[root];
  root_super_pages.insert(root_super_pages.end(), super_pages.begin(),
                          super_pages.end());
}

void PCScanInternal::RegisterNewSuperPage(Root* root,
                                          uintptr_t super_page_base) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  PA_DCHECK(!(super_page_base % kSuperPageAlignment));

  std::lock_guard<std::mutex> lock(roots_mutex_);

  // Dispatch based on whether root is scannable or not.
  if (root->IsScanEnabled()) {
    PA_DCHECK(scannable_roots_.count(root));
    auto& super_pages = scannable_roots_[root];
    PA_DCHECK(std::find(super_pages.begin(), super_pages.end(),
                        super_page_base) == super_pages.end());
    super_pages.push_back(super_page_base);
  } else {
    PA_DCHECK(root->IsQuarantineEnabled());
    PA_DCHECK(nonscannable_roots_.count(root));
    auto& super_pages = nonscannable_roots_[root];
    PA_DCHECK(std::find(super_pages.begin(), super_pages.end(),
                        super_page_base) == super_pages.end());
    super_pages.push_back(super_page_base);
  }
}

void PCScanInternal::SetProcessName(const char* process_name) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(process_name);
  PA_DCHECK(!process_name_);
  process_name_ = process_name;
}

size_t PCScanInternal::CalculateTotalHeapSize() const {
  PA_DCHECK(is_initialized());
  std::lock_guard<std::mutex> lock(roots_mutex_);
  const auto acc = [](size_t size, const auto& pair) {
    return size + pair.first->get_total_size_of_committed_pages();
  };
  return std::accumulate(scannable_roots_.begin(), scannable_roots_.end(), 0u,
                         acc) +
         std::accumulate(nonscannable_roots_.begin(), nonscannable_roots_.end(),
                         0u, acc);
}

void PCScanInternal::EnableStackScanning() {
  PA_DCHECK(!stack_scanning_enabled_);
  stack_scanning_enabled_ = true;
}
void PCScanInternal::DisableStackScanning() {
  PA_DCHECK(stack_scanning_enabled_);
  stack_scanning_enabled_ = false;
}
bool PCScanInternal::IsStackScanningEnabled() const {
  return stack_scanning_enabled_;
}

void PCScanInternal::NotifyThreadCreated(void* stack_top) {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  const auto res = stack_tops_.insert({tid, stack_top});
  PA_DCHECK(res.second);
}

void PCScanInternal::NotifyThreadDestroyed() {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  PA_DCHECK(1 == stack_tops_.count(tid));
  stack_tops_.erase(tid);
}

void* PCScanInternal::GetCurrentThreadStackTop() const {
  const auto tid = base::PlatformThread::CurrentId();
  std::lock_guard<std::mutex> lock(stack_tops_mutex_);
  auto it = stack_tops_.find(tid);
  return it != stack_tops_.end() ? it->second : nullptr;
}

bool PCScanInternal::WriteProtectionEnabled() const {
  return write_protector_->IsEnabled();
}

void PCScanInternal::ProtectPages(uintptr_t begin, size_t size) {
  // Slot-span sizes are multiple of system page size. However, the ranges that
  // are recorded are not, since in the snapshot we only record the used
  // payload. Therefore we align up the incoming range by 4k. The unused part of
  // slot-spans doesn't need to be protected (the allocator will enter the
  // safepoint before trying to allocate from it).
  PA_SCAN_DCHECK(write_protector_.get());
  write_protector_->ProtectPages(begin,
                                 base::bits::AlignUp(size, SystemPageSize()));
}

void PCScanInternal::UnprotectPages(uintptr_t begin, size_t size) {
  PA_SCAN_DCHECK(write_protector_.get());
  write_protector_->UnprotectPages(begin,
                                   base::bits::AlignUp(size, SystemPageSize()));
}

void PCScanInternal::ClearRootsForTesting() {
  std::lock_guard<std::mutex> lock(roots_mutex_);
  // Set all roots as non-scannable and non-quarantinable.
  for (auto& pair : scannable_roots_) {
    Root* root = pair.first;
    root->scan_mode = Root::ScanMode::kDisabled;
    root->quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  for (auto& pair : nonscannable_roots_) {
    Root* root = pair.first;
    root->quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  // Make sure to destroy maps so that on the following ReinitForTesting() call
  // the maps don't attempt to destroy the backing.
  scannable_roots_.clear();
  scannable_roots_.~RootsMap();
  nonscannable_roots_.clear();
  nonscannable_roots_.~RootsMap();
  // Destroy write protector object, so that there is no double free on the next
  // call to ReinitForTesting();
  write_protector_.reset();
}

void PCScanInternal::ReinitForTesting(PCScan::InitConfig config) {
  is_initialized_ = false;
  auto* new_this = new (this) PCScanInternal;
  new_this->Initialize(config);
}

void PCScanInternal::FinishScanForTesting() {
  auto current_task = CurrentPCScanTask();
  PA_CHECK(current_task.get());
  current_task->RunFromScanner();
}

}  // namespace internal
}  // namespace base
