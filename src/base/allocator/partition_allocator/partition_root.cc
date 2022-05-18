// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"

#include <cstdint>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "wow64apiset.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>
#endif

#if BUILDFLAG(RECORD_ALLOC_INFO)
namespace partition_alloc::internal {

// Even if this is not hidden behind a BUILDFLAG, it should not use any memory
// when recording is disabled, since it ends up in the .bss section.
AllocInfo g_allocs = {};

void RecordAllocOrFree(uintptr_t addr, size_t size) {
  g_allocs.allocs[g_allocs.index.fetch_add(1, std::memory_order_relaxed) %
                  kAllocInfoSize] = {addr, size};
}

}  // namespace partition_alloc::internal
#endif  // BUILDFLAG(RECORD_ALLOC_INFO)

namespace partition_alloc {

#if defined(PA_USE_PARTITION_ROOT_ENUMERATOR)

namespace {

internal::Lock g_root_enumerator_lock;
}

template <bool thread_safe>
internal::Lock& PartitionRoot<thread_safe>::GetEnumeratorLock() {
  return g_root_enumerator_lock;
}

namespace internal {

class PartitionRootEnumerator {
 public:
  using EnumerateCallback = void (*)(ThreadSafePartitionRoot* root,
                                     bool in_child);
  enum EnumerateOrder {
    kNormal,
    kReverse,
  };

  static PartitionRootEnumerator& Instance() {
    static PartitionRootEnumerator instance;
    return instance;
  }

  void Enumerate(EnumerateCallback callback,
                 bool in_child,
                 EnumerateOrder order) NO_THREAD_SAFETY_ANALYSIS {
    if (order == kNormal) {
      ThreadSafePartitionRoot* root;
      for (root = Head(partition_roots_); root != nullptr;
           root = root->next_root)
        callback(root, in_child);
    } else {
      PA_DCHECK(order == kReverse);
      ThreadSafePartitionRoot* root;
      for (root = Tail(partition_roots_); root != nullptr;
           root = root->prev_root)
        callback(root, in_child);
    }
  }

  void Register(ThreadSafePartitionRoot* root) {
    internal::ScopedGuard guard(ThreadSafePartitionRoot::GetEnumeratorLock());
    root->next_root = partition_roots_;
    root->prev_root = nullptr;
    if (partition_roots_)
      partition_roots_->prev_root = root;
    partition_roots_ = root;
  }

  void Unregister(ThreadSafePartitionRoot* root) {
    internal::ScopedGuard guard(ThreadSafePartitionRoot::GetEnumeratorLock());
    ThreadSafePartitionRoot* prev = root->prev_root;
    ThreadSafePartitionRoot* next = root->next_root;
    if (prev) {
      PA_DCHECK(prev->next_root == root);
      prev->next_root = next;
    } else {
      PA_DCHECK(partition_roots_ == root);
      partition_roots_ = next;
    }
    if (next) {
      PA_DCHECK(next->prev_root == root);
      next->prev_root = prev;
    }
    root->next_root = nullptr;
    root->prev_root = nullptr;
  }

 private:
  constexpr PartitionRootEnumerator() = default;

  ThreadSafePartitionRoot* Head(ThreadSafePartitionRoot* roots) {
    return roots;
  }

  ThreadSafePartitionRoot* Tail(ThreadSafePartitionRoot* roots)
      NO_THREAD_SAFETY_ANALYSIS {
    if (!roots)
      return nullptr;
    ThreadSafePartitionRoot* node = roots;
    for (; node->next_root != nullptr; node = node->next_root)
      ;
    return node;
  }

  ThreadSafePartitionRoot* partition_roots_
      GUARDED_BY(ThreadSafePartitionRoot::GetEnumeratorLock()) = nullptr;
};

}  // namespace internal

#endif  // PA_USE_PARTITION_ROOT_ENUMERATOR

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

#if defined(PA_HAS_ATFORK_HANDLER)

void LockRoot(PartitionRoot<internal::ThreadSafe>* root,
              bool) NO_THREAD_SAFETY_ANALYSIS {
  PA_DCHECK(root);
  root->lock_.Acquire();
}

// NO_THREAD_SAFETY_ANALYSIS: acquires the lock and doesn't release it, by
// design.
void BeforeForkInParent() NO_THREAD_SAFETY_ANALYSIS {
  // ThreadSafePartitionRoot::GetLock() is private. So use
  // g_root_enumerator_lock here.
  g_root_enumerator_lock.Acquire();
  internal::PartitionRootEnumerator::Instance().Enumerate(
      LockRoot, false,
      internal::PartitionRootEnumerator::EnumerateOrder::kNormal);

  ThreadCacheRegistry::GetLock().Acquire();
}

template <typename T>
void UnlockOrReinit(T& lock, bool in_child) NO_THREAD_SAFETY_ANALYSIS {
  // Only re-init the locks in the child process, in the parent can unlock
  // normally.
  if (in_child)
    lock.Reinit();
  else
    lock.Release();
}

void UnlockOrReinitRoot(PartitionRoot<internal::ThreadSafe>* root,
                        bool in_child) NO_THREAD_SAFETY_ANALYSIS {
  UnlockOrReinit(root->lock_, in_child);
}

void ReleaseLocks(bool in_child) NO_THREAD_SAFETY_ANALYSIS {
  // In reverse order, even though there are no lock ordering dependencies.
  UnlockOrReinit(ThreadCacheRegistry::GetLock(), in_child);
  internal::PartitionRootEnumerator::Instance().Enumerate(
      UnlockOrReinitRoot, in_child,
      internal::PartitionRootEnumerator::EnumerateOrder::kReverse);

  // ThreadSafePartitionRoot::GetLock() is private. So use
  // g_root_enumerator_lock here.
  UnlockOrReinit(g_root_enumerator_lock, in_child);
}

void AfterForkInParent() {
  ReleaseLocks(/* in_child = */ false);
}

void AfterForkInChild() {
  ReleaseLocks(/* in_child = */ true);
  // Unsafe, as noted in the name. This is fine here however, since at this
  // point there is only one thread, this one (unless another post-fork()
  // handler created a thread, but it would have needed to allocate, which would
  // have deadlocked the process already).
  //
  // If we don't reclaim this memory, it is lost forever. Note that this is only
  // really an issue if we fork() a multi-threaded process without calling
  // exec() right away, which is discouraged.
  ThreadCacheRegistry::Instance().ForcePurgeAllThreadAfterForkUnsafe();
}
#endif  // defined(PA_HAS_ATFORK_HANDLER)

std::atomic<bool> g_global_init_called;
void PartitionAllocMallocInitOnce() {
  bool expected = false;
  // No need to block execution for potential concurrent initialization, merely
  // want to make sure this is only called once.
  if (!g_global_init_called.compare_exchange_strong(expected, true))
    return;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // When fork() is called, only the current thread continues to execute in the
  // child process. If the lock is held, but *not* by this thread when fork() is
  // called, we have a deadlock.
  //
  // The "solution" here is to acquire the lock on the forking thread before
  // fork(), and keep it held until fork() is done, in the parent and the
  // child. To clean up memory, we also must empty the thread caches in the
  // child, which is easier, since no threads except for the current one are
  // running right after the fork().
  //
  // This is not perfect though, since:
  // - Multiple pre/post-fork() handlers can be registered, they are then run in
  //   LIFO order for the pre-fork handler, and FIFO order for the post-fork
  //   one. So unless we are the first to register a handler, if another handler
  //   allocates, then we deterministically deadlock.
  // - pthread handlers are *not* called when the application calls clone()
  //   directly, which is what Chrome does to launch processes.
  //
  // However, no perfect solution really exists to make threads + fork()
  // cooperate, but deadlocks are real (and fork() is used in DEATH_TEST()s),
  // and other malloc() implementations use the same techniques.
  int err =
      pthread_atfork(BeforeForkInParent, AfterForkInParent, AfterForkInChild);
  PA_CHECK(err == 0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

#if BUILDFLAG(IS_APPLE)
void PartitionAllocMallocHookOnBeforeForkInParent() {
  BeforeForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInParent() {
  AfterForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInChild() {
  AfterForkInChild();
}
#endif  // BUILDFLAG(IS_APPLE)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace internal {

template <bool thread_safe>
static size_t PartitionPurgeSlotSpan(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    bool discard) {
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  const internal::PartitionBucket<thread_safe>* bucket = slot_span->bucket;
  size_t slot_size = bucket->slot_size;
  if (slot_size < SystemPageSize() || !slot_span->num_allocated_slots)
    return 0;

  size_t bucket_num_slots = bucket->get_slots_per_span();
  size_t discardable_bytes = 0;

  if (slot_span->CanStoreRawSize()) {
    uint32_t utilized_slot_size = static_cast<uint32_t>(
        RoundUpToSystemPage(slot_span->GetUtilizedSlotSize()));
    discardable_bytes = bucket->slot_size - utilized_slot_size;
    if (discardable_bytes && discard) {
      uintptr_t slot_span_start =
          internal::SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
      uintptr_t committed_data_end = slot_span_start + utilized_slot_size;
      ScopedSyscallTimer timer{root};
      DiscardSystemPages(committed_data_end, discardable_bytes);
    }
    return discardable_bytes;
  }

#if defined(PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR)
  constexpr size_t kMaxSlotCount =
      (PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan) /
      SystemPageSize();
#elif BUILDFLAG(IS_APPLE) || (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64))
  // It's better for slot_usage to be stack-allocated and fixed-size, which
  // demands that its size be constexpr. On IS_APPLE and Linux on arm64,
  // PartitionPageSize() is always SystemPageSize() << 2, so regardless of
  // what the run time page size is, kMaxSlotCount can always be simplified
  // to this expression.
  constexpr size_t kMaxSlotCount =
      4 * internal::kMaxPartitionPagesPerRegularSlotSpan;
  PA_CHECK(kMaxSlotCount == (PartitionPageSize() *
                             internal::kMaxPartitionPagesPerRegularSlotSpan) /
                                SystemPageSize());
#endif
  PA_DCHECK(bucket_num_slots <= kMaxSlotCount);
  PA_DCHECK(slot_span->num_unprovisioned_slots < bucket_num_slots);
  size_t num_slots = bucket_num_slots - slot_span->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !BUILDFLAG(IS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_slots);
  uintptr_t slot_span_start =
      SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
  // First, walk the freelist for this slot span and make a bitmap of which
  // slots are not in use.
  for (PartitionFreelistEntry* entry = slot_span->get_freelist_head(); entry;
       /**/) {
    size_t slot_index = (::partition_alloc::internal::UnmaskPtr(
                             reinterpret_cast<uintptr_t>(entry)) -
                         slot_span_start) /
                        slot_size;
    PA_DCHECK(slot_index < num_slots);
    slot_usage[slot_index] = 0;
#if !BUILDFLAG(IS_WIN)
    // If we have a slot where the encoded next pointer is 0, we can actually
    // discard that entry because touching a discarded page is guaranteed to
    // return the original content or 0. (Note that this optimization won't be
    // effective on big-endian machines because the masking function is
    // negation.)
    if (entry->IsEncodedNextPtrZero())
      last_slot = slot_index;
#endif
    entry = entry->GetNext(slot_size);
  }

  // If the slot(s) at the end of the slot span are not in used, we can truncate
  // them entirely and rewrite the freelist.
  size_t truncated_slots = 0;
  while (!slot_usage[num_slots - 1]) {
    truncated_slots++;
    num_slots--;
    PA_DCHECK(num_slots);
  }
  // First, do the work of calculating the discardable bytes. Don't actually
  // discard anything unless the discard flag was passed in.
  if (truncated_slots) {
    size_t unprovisioned_bytes = 0;
    uintptr_t begin_addr = slot_span_start + (num_slots * slot_size);
    uintptr_t end_addr = begin_addr + (slot_size * truncated_slots);
    begin_addr = RoundUpToSystemPage(begin_addr);
    // We round the end address here up and not down because we're at the end of
    // a slot span, so we "own" all the way up the page boundary.
    end_addr = RoundUpToSystemPage(end_addr);
    PA_DCHECK(end_addr <= slot_span_start + bucket->get_bytes_per_span());
    if (begin_addr < end_addr) {
      unprovisioned_bytes = end_addr - begin_addr;
      discardable_bytes += unprovisioned_bytes;
    }
    if (unprovisioned_bytes && discard) {
      PA_DCHECK(truncated_slots > 0);
      size_t new_unprovisioned_slots =
          truncated_slots + slot_span->num_unprovisioned_slots;
      PA_DCHECK(new_unprovisioned_slots <= bucket->get_slots_per_span());
      slot_span->num_unprovisioned_slots = new_unprovisioned_slots;

      // Rewrite the freelist.
      internal::PartitionFreelistEntry* head = nullptr;
      internal::PartitionFreelistEntry* back = head;
      size_t num_new_entries = 0;
      for (size_t slot_index = 0; slot_index < num_slots; ++slot_index) {
        if (slot_usage[slot_index])
          continue;

        auto* entry = PartitionFreelistEntry::EmplaceAndInitNull(
            slot_span_start + (slot_size * slot_index));
        if (!head) {
          head = entry;
          back = entry;
        } else {
          back->SetNext(entry);
          back = entry;
        }
        num_new_entries++;
#if !BUILDFLAG(IS_WIN)
        last_slot = slot_index;
#endif
      }

      slot_span->SetFreelistHead(head);

      PA_DCHECK(num_new_entries == num_slots - slot_span->num_allocated_slots);
      // Discard the memory.
      ScopedSyscallTimer timer{root};
      DiscardSystemPages(begin_addr, unprovisioned_bytes);
    }
  }

  // Next, walk the slots and for any not in use, consider where the system page
  // boundaries occur. We can release any system pages back to the system as
  // long as we don't interfere with a freelist pointer or an adjacent slot.
  for (size_t i = 0; i < num_slots; ++i) {
    if (slot_usage[i])
      continue;
    // The first address we can safely discard is just after the freelist
    // pointer. There's one quirk: if the freelist pointer is actually nullptr,
    // we can discard that pointer value too.
    uintptr_t begin_addr = slot_span_start + (i * slot_size);
    uintptr_t end_addr = begin_addr + slot_size;
#if !BUILDFLAG(IS_WIN)
    if (i != last_slot)
      begin_addr += sizeof(internal::PartitionFreelistEntry);
#else
    begin_addr += sizeof(internal::PartitionFreelistEntry);
#endif
    begin_addr = RoundUpToSystemPage(begin_addr);
    end_addr = RoundDownToSystemPage(end_addr);
    if (begin_addr < end_addr) {
      size_t partial_slot_bytes = end_addr - begin_addr;
      discardable_bytes += partial_slot_bytes;
      if (discard) {
        ScopedSyscallTimer timer{root};
        DiscardSystemPages(begin_addr, partial_slot_bytes);
      }
    }
  }
  return discardable_bytes;
}

template <bool thread_safe>
static void PartitionPurgeBucket(
    internal::PartitionBucket<thread_safe>* bucket) {
  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<thread_safe>* slot_span =
             bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
      PartitionPurgeSlotSpan(slot_span, true);
    }
  }
}

template <bool thread_safe>
static void PartitionDumpSlotSpanStats(
    PartitionBucketMemoryStats* stats_out,
    internal::SlotSpanMetadata<thread_safe>* slot_span) {
  uint16_t bucket_num_slots = slot_span->bucket->get_slots_per_span();

  if (slot_span->is_decommitted()) {
    ++stats_out->num_decommitted_slot_spans;
    return;
  }

  stats_out->discardable_bytes += PartitionPurgeSlotSpan(slot_span, false);

  if (slot_span->CanStoreRawSize()) {
    stats_out->active_bytes += static_cast<uint32_t>(slot_span->GetRawSize());
    stats_out->active_count += 1;
  } else {
    stats_out->active_bytes +=
        (slot_span->num_allocated_slots * stats_out->bucket_slot_size);
    stats_out->active_count += slot_span->num_allocated_slots;
  }

  size_t slot_span_bytes_resident = RoundUpToSystemPage(
      (bucket_num_slots - slot_span->num_unprovisioned_slots) *
      stats_out->bucket_slot_size);
  stats_out->resident_bytes += slot_span_bytes_resident;
  if (slot_span->is_empty()) {
    stats_out->decommittable_bytes += slot_span_bytes_resident;
    ++stats_out->num_empty_slot_spans;
  } else if (slot_span->is_full()) {
    ++stats_out->num_full_slot_spans;
  } else {
    PA_DCHECK(slot_span->is_active());
    ++stats_out->num_active_slot_spans;
  }
}

template <bool thread_safe>
static void PartitionDumpBucketStats(
    PartitionBucketMemoryStats* stats_out,
    const internal::PartitionBucket<thread_safe>* bucket) {
  PA_DCHECK(!bucket->is_direct_mapped());
  stats_out->is_valid = false;
  // If the active slot span list is empty (==
  // internal::SlotSpanMetadata::get_sentinel_slot_span()), the bucket might
  // still need to be reported if it has a list of empty, decommitted or full
  // slot spans.
  if (bucket->active_slot_spans_head ==
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span() &&
      !bucket->empty_slot_spans_head && !bucket->decommitted_slot_spans_head &&
      !bucket->num_full_slot_spans)
    return;

  memset(stats_out, '\0', sizeof(*stats_out));
  stats_out->is_valid = true;
  stats_out->is_direct_map = false;
  stats_out->num_full_slot_spans =
      static_cast<size_t>(bucket->num_full_slot_spans);
  stats_out->bucket_slot_size = bucket->slot_size;
  uint16_t bucket_num_slots = bucket->get_slots_per_span();
  size_t bucket_useful_storage = stats_out->bucket_slot_size * bucket_num_slots;
  stats_out->allocated_slot_span_size = bucket->get_bytes_per_span();
  stats_out->active_bytes = bucket->num_full_slot_spans * bucket_useful_storage;
  stats_out->active_count = bucket->num_full_slot_spans * bucket_num_slots;
  stats_out->resident_bytes =
      bucket->num_full_slot_spans * stats_out->allocated_slot_span_size;

  for (internal::SlotSpanMetadata<thread_safe>* slot_span =
           bucket->empty_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_empty() || slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, slot_span);
  }
  for (internal::SlotSpanMetadata<thread_safe>* slot_span =
           bucket->decommitted_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, slot_span);
  }

  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<thread_safe>* slot_span =
             bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
      PartitionDumpSlotSpanStats(stats_out, slot_span);
    }
  }
}

#if DCHECK_IS_ON()
void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address) {
  PA_DCHECK(IsManagedByPartitionAllocBRPPool(address));
}
#endif

}  // namespace internal

template <bool thread_safe>
[[noreturn]] NOINLINE void PartitionRoot<thread_safe>::OutOfMemory(
    size_t size) {
  const size_t virtual_address_space_size =
      total_size_of_super_pages.load(std::memory_order_relaxed) +
      total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
#if !defined(ARCH_CPU_64_BITS)
  const size_t uncommitted_size =
      virtual_address_space_size -
      total_size_of_committed_pages.load(std::memory_order_relaxed);

  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  if (uncommitted_size > internal::kReasonableSizeOfUnusedPages) {
    internal::PartitionOutOfMemoryWithLotsOfUncommitedPages(size);
  }

#if BUILDFLAG(IS_WIN)
  // If true then we are running on 64-bit Windows.
  BOOL is_wow_64 = FALSE;
  // Intentionally ignoring failures.
  IsWow64Process(GetCurrentProcess(), &is_wow_64);
  // 32-bit address space on Windows is typically either 2 GiB (on 32-bit
  // Windows) or 4 GiB (on 64-bit Windows). 2.8 and 1.0 GiB are just rough
  // guesses as to how much address space PA can consume (note that code,
  // stacks, and other allocators will also consume address space).
  const size_t kReasonableVirtualSize = (is_wow_64 ? 2800 : 1024) * 1024 * 1024;
  // Make it obvious whether we are running on 64-bit Windows.
  PA_DEBUG_DATA_ON_STACK("is_wow_64", static_cast<size_t>(is_wow_64));
#else
  constexpr size_t kReasonableVirtualSize =
      // 1.5GiB elsewhere, since address space is typically 3GiB.
      (1024 + 512) * 1024 * 1024;
#endif
  if (virtual_address_space_size > kReasonableVirtualSize) {
    internal::PartitionOutOfMemoryWithLargeVirtualSize(
        virtual_address_space_size);
  }
#endif  // #if !defined(ARCH_CPU_64_BITS)

  // Out of memory can be due to multiple causes, such as:
  // - Out of GigaCage virtual address space
  // - Out of commit due to either our process, or another one
  // - Excessive allocations in the current process
  //
  // Saving these values make it easier to distinguish between these. See the
  // documentation in PA_DEBUG_DATA_ON_STACK() on how to get these from
  // minidumps.
  PA_DEBUG_DATA_ON_STACK("va_size", virtual_address_space_size);
  PA_DEBUG_DATA_ON_STACK("alloc", get_total_size_of_allocated_bytes());
  PA_DEBUG_DATA_ON_STACK("commit", get_total_size_of_committed_pages());
  PA_DEBUG_DATA_ON_STACK("size", size);

  if (internal::g_oom_handling_function)
    (*internal::g_oom_handling_function)(size);
  OOM_CRASH(size);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DecommitEmptySlotSpans() {
  ShrinkEmptySlotSpansRing(0);
  // Just decommitted everything, and holding the lock, should be exactly 0.
  PA_DCHECK(empty_slot_spans_dirty_bytes == 0);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DestructForTesting() {
  // We need to destruct the thread cache before we unreserve any of the super
  // pages below, which we currently are not doing. So, we should only call
  // this function on PartitionRoots without a thread cache.
  PA_CHECK(!with_thread_cache);
  auto pool_handle = ChoosePool();
  auto* curr = first_extent;
  while (curr != nullptr) {
    auto* next = curr->next;
    internal::AddressPoolManager::GetInstance()->UnreserveAndDecommit(
        pool_handle, reinterpret_cast<uintptr_t>(curr),
        internal::kSuperPageSize * curr->number_of_consecutive_super_pages);
    curr = next;
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::Init(PartitionOptions opts) {
  {
#if BUILDFLAG(IS_APPLE)
    // Needed to statically bound page size, which is a runtime constant on
    // apple OSes.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)));
#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
    // Check runtime pagesize. Though the code is currently the same, it is
    // not merged with the IS_APPLE case above as a 1 << 16 case needs to be
    // added here in the future, to allow 64 kiB pagesize. That is only
    // supported on Linux on arm64, not on IS_APPLE, but not yet present here
    // as the rest of the partition allocator does not currently support it.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)));
#endif

    ::partition_alloc::internal::ScopedGuard guard{lock_};
    if (initialized)
      return;

    // Swaps out the active no-op tagging intrinsics with MTE-capable ones, if
    // running on the right hardware.
    ::partition_alloc::internal::InitializeMTESupportIfNeeded();

#if defined(PA_HAS_64_BITS_POINTERS)
    // Reserve address space for partition alloc.
    internal::PartitionAddressSpace::Init();
#endif

    allow_aligned_alloc =
        opts.aligned_alloc == PartitionOptions::AlignedAlloc::kAllowed;
    allow_cookie = opts.cookie == PartitionOptions::Cookie::kAllowed;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    brp_enabled_ =
        opts.backup_ref_ptr == PartitionOptions::BackupRefPtr::kEnabled;
#else
    PA_CHECK(opts.backup_ref_ptr == PartitionOptions::BackupRefPtr::kDisabled);
#endif
    use_configurable_pool =
        (opts.use_configurable_pool ==
         PartitionOptions::UseConfigurablePool::kIfAvailable) &&
        IsConfigurablePoolAvailable();
    PA_DCHECK(!use_configurable_pool || IsConfigurablePoolAvailable());

    // brp_enabled_() is not supported in the configurable pool because
    // BRP requires objects to be in a different Pool.
    PA_CHECK(!(use_configurable_pool && brp_enabled()));

    // Ref-count messes up alignment needed for AlignedAlloc, making this
    // option incompatible. However, except in the
    // PUT_REF_COUNT_IN_PREVIOUS_SLOT case.
#if BUILDFLAG(USE_BACKUP_REF_PTR) && !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
    PA_CHECK(!allow_aligned_alloc || !brp_enabled_);
#endif

#if defined(PA_EXTRAS_REQUIRED)
    extras_size = 0;
    extras_offset = 0;

    if (allow_cookie) {
      extras_size += internal::kPartitionCookieSizeAdjustment;
    }

    if (brp_enabled()) {
      // TODO(tasak): In the PUT_REF_COUNT_IN_PREVIOUS_SLOT case, ref-count is
      // stored out-of-line for single-slot slot spans, so no need to
      // add/subtract its size in this case.
      extras_size += internal::kPartitionRefCountSizeAdjustment;
      extras_offset += internal::kPartitionRefCountOffsetAdjustment;
    }
#endif  //  defined(PA_EXTRAS_REQUIRED)

    // Re-confirm the above PA_CHECKs, by making sure there are no
    // pre-allocation extras when AlignedAlloc is allowed. Post-allocation
    // extras are ok.
    PA_CHECK(!allow_aligned_alloc || !extras_offset);

    quarantine_mode =
#if defined(PA_ALLOW_PCSCAN)
        (opts.quarantine == PartitionOptions::Quarantine::kDisallowed
             ? QuarantineMode::kAlwaysDisabled
             : QuarantineMode::kDisabledByDefault);
#else
        QuarantineMode::kAlwaysDisabled;
#endif  // defined(PA_ALLOW_PCSCAN)

    // We mark the sentinel slot span as free to make sure it is skipped by our
    // logic to find a new active slot span.
    memset(&sentinel_bucket, 0, sizeof(sentinel_bucket));
    sentinel_bucket.active_slot_spans_head = SlotSpan::get_sentinel_slot_span();

    // This is a "magic" value so we can test if a root pointer is valid.
    inverted_self = ~reinterpret_cast<uintptr_t>(this);

    // Set up the actual usable buckets first.
    constexpr internal::BucketIndexLookup lookup{};
    size_t bucket_index = 0;
    while (lookup.bucket_sizes()[bucket_index] !=
           internal::kInvalidBucketSize) {
      buckets[bucket_index].Init(lookup.bucket_sizes()[bucket_index]);
      bucket_index++;
    }
    PA_DCHECK(bucket_index < internal::kNumBuckets);

    // Remaining buckets are not usable, and not real.
    for (size_t index = bucket_index; index < internal::kNumBuckets; index++) {
      // Cannot init with size 0 since it computes 1 / size, but make sure the
      // bucket is invalid.
      buckets[index].Init(internal::kInvalidBucketSize);
      buckets[index].active_slot_spans_head = nullptr;
      PA_DCHECK(!buckets[index].is_valid());
    }

#if !defined(PA_THREAD_CACHE_SUPPORTED)
    // TLS in ThreadCache not supported on other OSes.
    with_thread_cache = false;
#else
    ThreadCache::EnsureThreadSpecificDataInitialized();
    with_thread_cache =
        (opts.thread_cache == PartitionOptions::ThreadCache::kEnabled);

    if (with_thread_cache)
      ThreadCache::Init(this);
#endif  // !defined(PA_THREAD_CACHE_SUPPORTED)

#if defined(PA_USE_PARTITION_ROOT_ENUMERATOR)
    internal::PartitionRootEnumerator::Instance().Register(this);
#endif

    initialized = true;
  }

  // Called without the lock, might allocate.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PartitionAllocMallocInitOnce();
#endif
}

template <bool thread_safe>
PartitionRoot<thread_safe>::~PartitionRoot() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(!with_thread_cache)
      << "Must not destroy a partition with a thread cache";
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if defined(PA_USE_PARTITION_ROOT_ENUMERATOR)
  if (initialized)
    internal::PartitionRootEnumerator::Instance().Unregister(this);
#endif  // defined(PA_USE_PARTITION_ALLOC_ENUMERATOR)
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::EnableThreadCacheIfSupported() {
#if defined(PA_THREAD_CACHE_SUPPORTED)
  ::partition_alloc::internal::ScopedGuard guard{lock_};
  PA_CHECK(!with_thread_cache);
  // By the time we get there, there may be multiple threads created in the
  // process. Since `with_thread_cache` is accessed without a lock, it can
  // become visible to another thread before the effects of
  // `internal::ThreadCacheInit()` are visible. To prevent that, we fake thread
  // cache creation being in-progress while this is running.
  //
  // This synchronizes with the acquire load in `MaybeInitThreadCacheAndAlloc()`
  // to ensure that we don't create (and thus use) a ThreadCache before
  // ThreadCache::Init()'s effects are visible.
  int before =
      thread_caches_being_constructed_.fetch_add(1, std::memory_order_acquire);
  PA_CHECK(before == 0);
  ThreadCache::Init(this);
  thread_caches_being_constructed_.fetch_sub(1, std::memory_order_release);
  with_thread_cache = true;
#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

template <bool thread_safe>
bool PartitionRoot<thread_safe>::TryReallocInPlaceForDirectMap(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    size_t requested_size) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  PA_DCHECK(
      internal::IsManagedByDirectMap(reinterpret_cast<uintptr_t>(slot_span)));

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  auto* extent = DirectMapExtent::FromSlotSpan(slot_span);
  size_t current_reservation_size = extent->reservation_size;
  // Calculate the new reservation size the way PartitionDirectMap() would, but
  // skip the alignment, because this call isn't requesting it.
  size_t new_reservation_size = GetDirectMapReservationSize(raw_size);

  // If new reservation would be larger, there is nothing we can do to
  // reallocate in-place.
  if (new_reservation_size > current_reservation_size)
    return false;

  // Don't reallocate in-place if new reservation size would be less than 80 %
  // of the current one, to avoid holding on to too much unused address space.
  // Make this check before comparing slot sizes, as even with equal or similar
  // slot sizes we can save a lot if the original allocation was heavily padded
  // for alignment.
  if ((new_reservation_size >> internal::SystemPageShift()) * 5 <
      (current_reservation_size >> internal::SystemPageShift()) * 4)
    return false;

  // Note that the new size isn't a bucketed size; this function is called
  // whenever we're reallocating a direct mapped allocation, so calculate it
  // the way PartitionDirectMap() would.
  size_t new_slot_size = GetDirectMapSlotSize(raw_size);
  if (new_slot_size < internal::kMinDirectMappedDownsize)
    return false;

  // Past this point, we decided we'll attempt to reallocate without relocating,
  // so we have to honor the padding for alignment in front of the original
  // allocation, even though this function isn't requesting any alignment.

  // bucket->slot_size is the currently committed size of the allocation.
  size_t current_slot_size = slot_span->bucket->slot_size;
  uintptr_t slot_start = SlotSpan::ToSlotSpanStart(slot_span);
  // This is the available part of the reservation up to which the new
  // allocation can grow.
  size_t available_reservation_size =
      current_reservation_size - extent->padding_for_alignment -
      PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
#if DCHECK_IS_ON()
  uintptr_t reservation_start = slot_start & internal::kSuperPageBaseMask;
  PA_DCHECK(internal::IsReservationStart(reservation_start));
  PA_DCHECK(slot_start + available_reservation_size ==
            reservation_start + current_reservation_size -
                GetDirectMapMetadataAndGuardPagesSize() +
                internal::PartitionPageSize());
#endif

  if (new_slot_size == current_slot_size) {
    // No need to move any memory around, but update size and cookie below.
    // That's because raw_size may have changed.
  } else if (new_slot_size < current_slot_size) {
    // Shrink by decommitting unneeded pages and making them inaccessible.
    size_t decommit_size = current_slot_size - new_slot_size;
    DecommitSystemPagesForData(slot_start + new_slot_size, decommit_size,
                               PageAccessibilityDisposition::kRequireUpdate);
    // Since the decommited system pages are still reserved, we don't need to
    // change the entries for decommitted pages in the reservation offset table.
  } else if (new_slot_size <= available_reservation_size) {
    // Grow within the actually reserved address space. Just need to make the
    // pages accessible again.
    size_t recommit_slot_size_growth = new_slot_size - current_slot_size;
    RecommitSystemPagesForData(slot_start + current_slot_size,
                               recommit_slot_size_growth,
                               PageAccessibilityDisposition::kRequireUpdate);
    // The recommited system pages had been already reserved and all the
    // entries in the reservation offset table (for entire reservation_size
    // region) have been already initialized.

#if DCHECK_IS_ON()
    memset(reinterpret_cast<void*>(slot_start + current_slot_size),
           internal::kUninitializedByte, recommit_slot_size_growth);
#endif
  } else {
    // We can't perform the realloc in-place.
    // TODO: support this too when possible.
    return false;
  }

  DecreaseTotalSizeOfAllocatedBytes(reinterpret_cast<uintptr_t>(slot_span),
                                    slot_span->bucket->slot_size);
  slot_span->SetRawSize(raw_size);
  slot_span->bucket->slot_size = new_slot_size;
  IncreaseTotalSizeOfAllocatedBytes(reinterpret_cast<uintptr_t>(slot_span),
                                    slot_span->bucket->slot_size, raw_size);

#if DCHECK_IS_ON()
  // Write a new trailing cookie.
  if (allow_cookie) {
    auto* object =
        reinterpret_cast<unsigned char*>(SlotStartToObject(slot_start));
    internal::PartitionCookieWriteValue(object +
                                        slot_span->GetUsableSize(this));
  }
#endif

  return true;
}

template <bool thread_safe>
bool PartitionRoot<thread_safe>::TryReallocInPlaceForNormalBuckets(
    void* ptr,
    SlotSpan* slot_span,
    size_t new_size) {
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  PA_DCHECK(internal::IsManagedByNormalBuckets(address));

  // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
  // new size is a significant percentage smaller. We could do the same if we
  // determine it is a win.
  if (AllocationCapacityFromRequestedSize(new_size) !=
      AllocationCapacityFromPtr(ptr))
    return false;

  // Trying to allocate |new_size| would use the same amount of underlying
  // memory as we're already using, so re-use the allocation after updating
  // statistics (and cookie, if present).
  if (slot_span->CanStoreRawSize()) {
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && DCHECK_IS_ON()
    uintptr_t slot_start = ObjectToSlotStart(ptr);
    internal::PartitionRefCount* old_ref_count;
    if (brp_enabled()) {
      old_ref_count = internal::PartitionRefCountPointer(slot_start);
    }
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && DCHECK_IS_ON()
    size_t new_raw_size = AdjustSizeForExtrasAdd(new_size);
    slot_span->SetRawSize(new_raw_size);
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && DCHECK_IS_ON()
    if (brp_enabled()) {
      internal::PartitionRefCount* new_ref_count =
          internal::PartitionRefCountPointer(slot_start);
      PA_DCHECK(new_ref_count == old_ref_count);
    }
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && DCHECK_IS_ON()
#if DCHECK_IS_ON()
    // Write a new trailing cookie only when it is possible to keep track
    // raw size (otherwise we wouldn't know where to look for it later).
    if (allow_cookie) {
      internal::PartitionCookieWriteValue(
          reinterpret_cast<unsigned char*>(address) +
          slot_span->GetUsableSize(this));
    }
#endif  // DCHECK_IS_ON()
  }
  return ptr;
}

template <bool thread_safe>
void* PartitionRoot<thread_safe>::ReallocWithFlags(int flags,
                                                   void* ptr,
                                                   size_t new_size,
                                                   const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(new_size, flags);
  void* result = realloc(ptr, new_size);
  PA_CHECK(result || flags & AllocFlags::kReturnNull);
  return result;
#else
  bool no_hooks = flags & AllocFlags::kNoHooks;
  if (UNLIKELY(!ptr)) {
    return no_hooks
               ? AllocWithFlagsNoHooks(flags, new_size,
                                       internal::PartitionPageSize())
               : AllocWithFlagsInternal(
                     flags, new_size, internal::PartitionPageSize(), type_name);
  }

  if (UNLIKELY(!new_size)) {
    Free(ptr);
    return nullptr;
  }

  if (new_size > internal::MaxDirectMapped()) {
    if (flags & AllocFlags::kReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  bool overridden = false;
  size_t old_usable_size;
  if (UNLIKELY(!no_hooks && hooks_enabled)) {
    overridden = PartitionAllocHooks::ReallocOverrideHookIfEnabled(
        &old_usable_size, ptr);
  }
  if (LIKELY(!overridden)) {
    // |ptr| may have been allocated in another root.
    SlotSpan* slot_span = SlotSpan::FromObject(ptr);
    auto* old_root = PartitionRoot::FromSlotSpan(slot_span);
    bool success = false;
    bool tried_in_place_for_direct_map = false;
    {
      ::partition_alloc::internal::ScopedGuard guard{old_root->lock_};
      // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
      PA_DCHECK(IsValidSlotSpan(slot_span));
      old_usable_size = slot_span->GetUsableSize(old_root);

      if (UNLIKELY(slot_span->bucket->is_direct_mapped())) {
        tried_in_place_for_direct_map = true;
        // We may be able to perform the realloc in place by changing the
        // accessibility of memory pages and, if reducing the size, decommitting
        // them.
        success = old_root->TryReallocInPlaceForDirectMap(slot_span, new_size);
      }
    }
    if (success) {
      if (UNLIKELY(!no_hooks && hooks_enabled)) {
        PartitionAllocHooks::ReallocObserverHookIfEnabled(ptr, ptr, new_size,
                                                          type_name);
      }
      return ptr;
    }

    if (LIKELY(!tried_in_place_for_direct_map)) {
      if (old_root->TryReallocInPlaceForNormalBuckets(ptr, slot_span, new_size))
        return ptr;
    }
  }

  // This realloc cannot be resized in-place. Sadness.
  void* ret =
      no_hooks ? AllocWithFlagsNoHooks(flags, new_size,
                                       internal::PartitionPageSize())
               : AllocWithFlagsInternal(
                     flags, new_size, internal::PartitionPageSize(), type_name);
  if (!ret) {
    if (flags & AllocFlags::kReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  memcpy(ret, ptr, std::min(old_usable_size, new_size));
  Free(ptr);  // Implicitly protects the old ptr on MTE systems.
  return ret;
#endif
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::PurgeMemory(int flags) {
  {
    ::partition_alloc::internal::ScopedGuard guard{lock_};
    // Avoid purging if there is PCScan task currently scheduled. Since pcscan
    // takes snapshot of all allocated pages, decommitting pages here (even
    // under the lock) is racy.
    // TODO(bikineev): Consider rescheduling the purging after PCScan.
    if (PCScan::IsInProgress())
      return;

    if (flags & PurgeFlags::kDecommitEmptySlotSpans)
      DecommitEmptySlotSpans();
    if (flags & PurgeFlags::kDiscardUnusedSystemPages) {
      for (Bucket& bucket : buckets) {
        if (bucket.slot_size == internal::kInvalidBucketSize)
          continue;

        if (bucket.slot_size >= internal::SystemPageSize())
          internal::PartitionPurgeBucket(&bucket);
        else
          bucket.SortSlotSpanFreelists();

        // Do it at the end, as the actions above change the status of slot
        // spans (e.g. empty -> decommitted).
        bucket.MaintainActiveList();
      }
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::ShrinkEmptySlotSpansRing(size_t limit) {
  int16_t index = global_empty_slot_span_ring_index;
  int16_t starting_index = index;
  while (empty_slot_spans_dirty_bytes > limit) {
    SlotSpan* slot_span = global_empty_slot_span_ring[index];
    // The ring is not always full, may be nullptr.
    if (slot_span) {
      slot_span->DecommitIfPossible(this);
      global_empty_slot_span_ring[index] = nullptr;
    }
    index += 1;
    // Walk through the entirety of possible slots, even though the last ones
    // are unused, if global_empty_slot_span_ring_size is smaller than
    // kMaxFreeableSpans. It's simpler, and does not cost anything, since all
    // the pointers are going to be nullptr.
    if (index == internal::kMaxFreeableSpans)
      index = 0;

    // Went around the whole ring, since this is locked,
    // empty_slot_spans_dirty_bytes should be exactly 0.
    if (index == starting_index) {
      PA_DCHECK(empty_slot_spans_dirty_bytes == 0);
      // Metrics issue, don't crash, return.
      break;
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DumpStats(const char* partition_name,
                                           bool is_light_dump,
                                           PartitionStatsDumper* dumper) {
  static const size_t kMaxReportableDirectMaps = 4096;
  // Allocate on the heap rather than on the stack to avoid stack overflow
  // skirmishes (on Windows, in particular). Allocate before locking below,
  // otherwise when PartitionAlloc is malloc() we get reentrancy issues. This
  // inflates reported values a bit for detailed dumps though, by 16kiB.
  std::unique_ptr<uint32_t[]> direct_map_lengths;
  if (!is_light_dump) {
    direct_map_lengths =
        std::unique_ptr<uint32_t[]>(new uint32_t[kMaxReportableDirectMaps]);
  }
  PartitionBucketMemoryStats bucket_stats[internal::kNumBuckets];
  size_t num_direct_mapped_allocations = 0;
  PartitionMemoryStats stats = {0};

  stats.syscall_count = syscall_count.load(std::memory_order_relaxed);
  stats.syscall_total_time_ns =
      syscall_total_time_ns.load(std::memory_order_relaxed);

  // Collect data with the lock held, cannot allocate or call third-party code
  // below.
  {
    ::partition_alloc::internal::ScopedGuard guard{lock_};
    PA_DCHECK(total_size_of_allocated_bytes <= max_size_of_allocated_bytes);

    stats.total_mmapped_bytes =
        total_size_of_super_pages.load(std::memory_order_relaxed) +
        total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
    stats.total_committed_bytes =
        total_size_of_committed_pages.load(std::memory_order_relaxed);
    stats.max_committed_bytes =
        max_size_of_committed_pages.load(std::memory_order_relaxed);
    stats.total_allocated_bytes = total_size_of_allocated_bytes;
    stats.max_allocated_bytes = max_size_of_allocated_bytes;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    stats.total_brp_quarantined_bytes =
        total_size_of_brp_quarantined_bytes.load(std::memory_order_relaxed);
    stats.total_brp_quarantined_count =
        total_count_of_brp_quarantined_slots.load(std::memory_order_relaxed);
#endif

    size_t direct_mapped_allocations_total_size = 0;
    for (size_t i = 0; i < internal::kNumBuckets; ++i) {
      const Bucket* bucket = &bucket_at(i);
      // Don't report the pseudo buckets that the generic allocator sets up in
      // order to preserve a fast size->bucket map (see
      // PartitionRoot::Init() for details).
      if (!bucket->is_valid())
        bucket_stats[i].is_valid = false;
      else
        internal::PartitionDumpBucketStats(&bucket_stats[i], bucket);
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
        stats.total_active_count += bucket_stats[i].active_count;
        stats.total_decommittable_bytes += bucket_stats[i].decommittable_bytes;
        stats.total_discardable_bytes += bucket_stats[i].discardable_bytes;
      }
    }

    for (DirectMapExtent* extent = direct_map_list;
         extent && num_direct_mapped_allocations < kMaxReportableDirectMaps;
         extent = extent->next_extent, ++num_direct_mapped_allocations) {
      PA_DCHECK(!extent->next_extent ||
                extent->next_extent->prev_extent == extent);
      size_t slot_size = extent->bucket->slot_size;
      direct_mapped_allocations_total_size += slot_size;
      if (is_light_dump)
        continue;
      direct_map_lengths[num_direct_mapped_allocations] = slot_size;
    }

    stats.total_resident_bytes += direct_mapped_allocations_total_size;
    stats.total_active_bytes += direct_mapped_allocations_total_size;
    stats.total_active_count += num_direct_mapped_allocations;

    stats.has_thread_cache = with_thread_cache;
    if (stats.has_thread_cache) {
      ThreadCacheRegistry::Instance().DumpStats(
          true, &stats.current_thread_cache_stats);
      ThreadCacheRegistry::Instance().DumpStats(false,
                                                &stats.all_thread_caches_stats);
    }
  }

  // Do not hold the lock when calling |dumper|, as it may allocate.
  if (!is_light_dump) {
    for (auto& stat : bucket_stats) {
      if (stat.is_valid)
        dumper->PartitionsDumpBucketStats(partition_name, &stat);
    }

    for (size_t i = 0; i < num_direct_mapped_allocations; ++i) {
      uint32_t size = direct_map_lengths[i];

      PartitionBucketMemoryStats mapped_stats = {};
      mapped_stats.is_valid = true;
      mapped_stats.is_direct_map = true;
      mapped_stats.num_full_slot_spans = 1;
      mapped_stats.allocated_slot_span_size = size;
      mapped_stats.bucket_slot_size = size;
      mapped_stats.active_bytes = size;
      mapped_stats.active_count = 1;
      mapped_stats.resident_bytes = size;
      dumper->PartitionsDumpBucketStats(partition_name, &mapped_stats);
    }
  }
  dumper->PartitionDumpTotals(partition_name, &stats);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DeleteForTesting(
    PartitionRoot* partition_root) {
  if (partition_root->with_thread_cache) {
    ThreadCache::SwapForTesting(nullptr);
    partition_root->with_thread_cache = false;
  }

  delete partition_root;
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::ResetBookkeepingForTesting() {
  ::partition_alloc::internal::ScopedGuard guard{lock_};
  max_size_of_allocated_bytes = total_size_of_allocated_bytes;
  max_size_of_committed_pages.store(total_size_of_committed_pages);
}

template <>
uintptr_t PartitionRoot<internal::ThreadSafe>::MaybeInitThreadCacheAndAlloc(
    uint16_t bucket_index,
    size_t* slot_size) {
  auto* tcache = ThreadCache::Get();
  // See comment in `EnableThreadCacheIfSupport()` for why this is an acquire
  // load.
  if (ThreadCache::IsTombstone(tcache) ||
      thread_caches_being_constructed_.load(std::memory_order_acquire)) {
    // Two cases:
    // 1. Thread is being terminated, don't try to use the thread cache, and
    //    don't try to resurrect it.
    // 2. Someone, somewhere is currently allocating a thread cache. This may
    //    be us, in which case we are re-entering and should not create a thread
    //    cache. If it is not us, then this merely delays thread cache
    //    construction a bit, which is not an issue.
    return 0;
  }

  // There is no per-thread ThreadCache allocated here yet, and this partition
  // has a thread cache, allocate a new one.
  //
  // The thread cache allocation itself will not reenter here, as it sidesteps
  // the thread cache by using placement new and |RawAlloc()|. However,
  // internally to libc, allocations may happen to create a new TLS
  // variable. This would end up here again, which is not what we want (and
  // likely is not supported by libc).
  //
  // To avoid this sort of reentrancy, increase the count of thread caches that
  // are currently allocating a thread cache.
  //
  // Note that there is no deadlock or data inconsistency concern, since we do
  // not hold the lock, and has such haven't touched any internal data.
  int before =
      thread_caches_being_constructed_.fetch_add(1, std::memory_order_relaxed);
  PA_CHECK(before < std::numeric_limits<int>::max());
  tcache = ThreadCache::Create(this);
  thread_caches_being_constructed_.fetch_sub(1, std::memory_order_relaxed);

  // Cache is created empty, but at least this will trigger batch fill, which
  // may be useful, and we are already in a slow path anyway (first small
  // allocation of this thread).
  return tcache->GetFromCache(bucket_index, slot_size);
}

template struct BASE_EXPORT PartitionRoot<internal::ThreadSafe>;

static_assert(offsetof(PartitionRoot<internal::ThreadSafe>, sentinel_bucket) ==
                  offsetof(PartitionRoot<internal::ThreadSafe>, buckets) +
                      internal::kNumBuckets *
                          sizeof(PartitionRoot<internal::ThreadSafe>::Bucket),
              "sentinel_bucket must be just after the regular buckets.");

static_assert(
    offsetof(PartitionRoot<internal::ThreadSafe>, lock_) >= 64,
    "The lock should not be on the same cacheline as the read-mostly flags");
}  // namespace partition_alloc
