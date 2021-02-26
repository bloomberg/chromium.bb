// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "build/build_config.h"

namespace base {

namespace internal {

template <bool thread_safe>
static size_t PartitionPurgeSlotSpan(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    bool discard) {
  const internal::PartitionBucket<thread_safe>* bucket = slot_span->bucket;
  size_t slot_size = bucket->slot_size;
  if (slot_size < SystemPageSize() || !slot_span->num_allocated_slots)
    return 0;

  size_t bucket_num_slots = bucket->get_slots_per_span();
  size_t discardable_bytes = 0;

  if (slot_span->CanStoreRawSize()) {
    uint32_t used_bytes =
        static_cast<uint32_t>(RoundUpToSystemPage(slot_span->GetRawSize()));
    discardable_bytes = bucket->slot_size - used_bytes;
    if (discardable_bytes && discard) {
      char* ptr = reinterpret_cast<char*>(
          internal::SlotSpanMetadata<thread_safe>::ToPointer(slot_span));
      ptr += used_bytes;
      DiscardSystemPages(ptr, discardable_bytes);
    }
    return discardable_bytes;
  }

#if defined(PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR)
  constexpr size_t kMaxSlotCount =
      (PartitionPageSize() * kMaxPartitionPagesPerSlotSpan) / SystemPageSize();
#elif defined(OS_APPLE)
  // It's better for slot_usage to be stack-allocated and fixed-size, which
  // demands that its size be constexpr. On OS_APPLE, PartitionPageSize() is
  // always SystemPageSize() << 2, so regardless of what the run time page size
  // is, kMaxSlotCount can always be simplified to this expression.
  constexpr size_t kMaxSlotCount = 4 * kMaxPartitionPagesPerSlotSpan;
  PA_CHECK(kMaxSlotCount ==
           (PartitionPageSize() * kMaxPartitionPagesPerSlotSpan) /
               SystemPageSize());
#endif
  PA_DCHECK(bucket_num_slots <= kMaxSlotCount);
  PA_DCHECK(slot_span->num_unprovisioned_slots < bucket_num_slots);
  size_t num_slots = bucket_num_slots - slot_span->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !defined(OS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_slots);
  char* ptr = reinterpret_cast<char*>(
      internal::SlotSpanMetadata<thread_safe>::ToPointer(slot_span));
  // First, walk the freelist for this slot span and make a bitmap of which
  // slots are not in use.
  for (internal::PartitionFreelistEntry* entry = slot_span->freelist_head;
       entry;
       /**/) {
    size_t slot_index = (reinterpret_cast<char*>(entry) - ptr) / slot_size;
    PA_DCHECK(slot_index < num_slots);
    slot_usage[slot_index] = 0;
    entry = entry->GetNext();
#if !defined(OS_WIN)
    // If we have a slot where the masked freelist entry is 0, we can actually
    // discard that freelist entry because touching a discarded page is
    // guaranteed to return original content or 0. (Note that this optimization
    // won't fire on big-endian machines because the masking function is
    // negation.)
    if (!internal::PartitionFreelistEntry::Encode(entry))
      last_slot = slot_index;
#endif
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
    char* begin_ptr = ptr + (num_slots * slot_size);
    char* end_ptr = begin_ptr + (slot_size * truncated_slots);
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    // We round the end pointer here up and not down because we're at the end of
    // a slot span, so we "own" all the way up the page boundary.
    end_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    PA_DCHECK(end_ptr <= ptr + bucket->get_bytes_per_span());
    if (begin_ptr < end_ptr) {
      unprovisioned_bytes = end_ptr - begin_ptr;
      discardable_bytes += unprovisioned_bytes;
    }
    if (unprovisioned_bytes && discard) {
      PA_DCHECK(truncated_slots > 0);
      size_t num_new_entries = 0;
      slot_span->num_unprovisioned_slots +=
          static_cast<uint16_t>(truncated_slots);

      // Rewrite the freelist.
      internal::PartitionFreelistEntry* head = nullptr;
      internal::PartitionFreelistEntry* back = head;
      for (size_t slot_index = 0; slot_index < num_slots; ++slot_index) {
        if (slot_usage[slot_index])
          continue;

        auto* entry = reinterpret_cast<internal::PartitionFreelistEntry*>(
            ptr + (slot_size * slot_index));
        if (!head) {
          head = entry;
          back = entry;
        } else {
          back->SetNext(entry);
          back = entry;
        }
        num_new_entries++;
#if !defined(OS_WIN)
        last_slot = slot_index;
#endif
      }

      slot_span->SetFreelistHead(head);
      if (back)
        back->SetNext(nullptr);

      PA_DCHECK(num_new_entries == num_slots - slot_span->num_allocated_slots);
      // Discard the memory.
      DiscardSystemPages(begin_ptr, unprovisioned_bytes);
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
    char* begin_ptr = ptr + (i * slot_size);
    char* end_ptr = begin_ptr + slot_size;
#if !defined(OS_WIN)
    if (i != last_slot)
      begin_ptr += sizeof(internal::PartitionFreelistEntry);
#else
    begin_ptr += sizeof(internal::PartitionFreelistEntry);
#endif
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    end_ptr = reinterpret_cast<char*>(
        RoundDownToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    if (begin_ptr < end_ptr) {
      size_t partial_slot_bytes = end_ptr - begin_ptr;
      discardable_bytes += partial_slot_bytes;
      if (discard)
        DiscardSystemPages(begin_ptr, partial_slot_bytes);
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
  } else {
    stats_out->active_bytes +=
        (slot_span->num_allocated_slots * stats_out->bucket_slot_size);
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

template <bool thread_safe>
void InitBucketIndexLookup(PartitionRoot<thread_safe>* root) {
  uint16_t* bucket_index_ptr =
      &PartitionRoot<thread_safe>::bucket_index_lookup[0];
  uint16_t bucket_index = 0;
  const uint16_t sentinel_bucket_index = kNumBuckets;

  for (uint16_t order = 0; order <= kBitsPerSizeT; ++order) {
    for (uint16_t j = 0; j < kNumBucketsPerOrder; ++j) {
      if (order < kMinBucketedOrder) {
        // Use the bucket of the finest granularity for malloc(0) etc.
        *bucket_index_ptr++ = 0;
      } else if (order > kMaxBucketedOrder) {
        *bucket_index_ptr++ = sentinel_bucket_index;
      } else {
        uint16_t valid_bucket_index = bucket_index;
        while (root->buckets[valid_bucket_index].slot_size % kSmallestBucket)
          valid_bucket_index++;
        *bucket_index_ptr++ = valid_bucket_index;
        bucket_index++;
      }
    }
  }
  PA_DCHECK(bucket_index == kNumBuckets);
  PA_DCHECK(bucket_index_ptr ==
            PartitionRoot<thread_safe>::bucket_index_lookup +
                ((kBitsPerSizeT + 1) * kNumBucketsPerOrder));
  // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
  // which tries to overflow to a non-existent order.
  *bucket_index_ptr = sentinel_bucket_index;
}

}  // namespace internal

// TODO(lizeb): Consider making this constexpr. Without C++17 std::array, this
// requires some template magic, only do it if it helps performance.
template <bool thread_safe>
uint16_t PartitionRoot<thread_safe>::bucket_index_lookup[((kBitsPerSizeT + 1) *
                                                          kNumBucketsPerOrder) +
                                                         1];

template <bool thread_safe>
NOINLINE void PartitionRoot<thread_safe>::OutOfMemory(size_t size) {
#if !defined(ARCH_CPU_64_BITS)
  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  //
  if (total_size_of_super_pages.load(std::memory_order_relaxed) +
          total_size_of_direct_mapped_pages.load(std::memory_order_relaxed) -
          total_size_of_committed_pages.load(std::memory_order_relaxed) >
      kReasonableSizeOfUnusedPages) {
    internal::PartitionOutOfMemoryWithLotsOfUncommitedPages(size);
  }
#endif
  if (internal::g_oom_handling_function)
    (*internal::g_oom_handling_function)(size);
  OOM_CRASH(size);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DecommitEmptySlotSpans() {
  for (SlotSpan*& slot_span : global_empty_slot_span_ring) {
    if (slot_span)
      slot_span->DecommitIfPossible(this);
    slot_span = nullptr;
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::Init(PartitionOptions opts) {
  ScopedGuard guard{lock_};
  if (initialized)
    return;

#if defined(PA_HAS_64_BITS_POINTERS)
  // Reserve address space for partition alloc.
  if (features::IsPartitionAllocGigaCageEnabled())
    internal::PartitionAddressSpace::Init();
#endif

  // If alignment needs to be enforced, disallow adding cookies and/or tags at
  // the beginning of the slot.
  allow_extras = (opts.alignment != PartitionOptions::Alignment::kAlignedAlloc);

#if ALLOW_ENABLING_PCSCAN
  scannable = (opts.pcscan != PartitionOptions::PCScan::kAlwaysDisabled);
#else
  scannable = false;
#endif
  // Concurrent freeing in PCScan can only safely work on thread-safe
  // partitions.
  if (thread_safe &&
      opts.pcscan == PartitionOptions::PCScan::kForcedEnabledForTesting)
    pcscan.emplace(this);

  // We mark the sentinel slot span as free to make sure it is skipped by our
  // logic to find a new active slot span.
  memset(&sentinel_bucket, 0, sizeof(sentinel_bucket));
  sentinel_bucket.active_slot_spans_head = SlotSpan::get_sentinel_slot_span();

  // This is a "magic" value so we can test if a root pointer is valid.
  inverted_self = ~reinterpret_cast<uintptr_t>(this);

  // Set up the actual usable buckets first.
  // Note that typical values (i.e. min allocation size of 8) will result in
  // pseudo buckets (size==9 etc. or more generally, size is not a multiple
  // of the smallest allocation granularity).
  // We avoid them in the bucket lookup map, but we tolerate them to keep the
  // code simpler and the structures more generic.
  size_t i, j;
  size_t current_size = kSmallestBucket;
  size_t current_increment = kSmallestBucket >> kNumBucketsPerOrderBits;
  Bucket* bucket = &buckets[0];
  for (i = 0; i < kNumBucketedOrders; ++i) {
    for (j = 0; j < kNumBucketsPerOrder; ++j) {
      bucket->Init(current_size);
      // Disable pseudo buckets so that touching them faults.
      if (current_size % kSmallestBucket)
        bucket->active_slot_spans_head = nullptr;
      current_size += current_increment;
      ++bucket;
    }
    current_increment <<= 1;
  }
  PA_DCHECK(current_size == 1 << kMaxBucketedOrder);
  PA_DCHECK(bucket == &buckets[0] + kNumBuckets);

  // Then set up the fast size -> bucket lookup table.  We call this multiple
  // times, even though the indices are shared between all PartitionRoots, but
  // this operation is idempotent, so there is no harm.
  internal::InitBucketIndexLookup(this);

#if !defined(PA_THREAD_CACHE_SUPPORTED)
  // TLS in ThreadCache not supported on other OSes.
  with_thread_cache = false;
#else
  with_thread_cache =
      (opts.thread_cache == PartitionOptions::ThreadCache::kEnabled);

  if (with_thread_cache)
    internal::ThreadCache::Init(this);
#endif  // !defined(PA_THREAD_CACHE_SUPPORTED)

  initialized = true;
}

template <bool thread_safe>
PartitionRoot<thread_safe>::~PartitionRoot() = default;

template <bool thread_safe>
bool PartitionRoot<thread_safe>::ReallocDirectMappedInPlace(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    size_t requested_size) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());

  size_t raw_size =
      internal::PartitionSizeAdjustAdd(allow_extras, requested_size);
  // Note that the new size isn't a bucketed size; this function is called
  // whenever we're reallocating a direct mapped allocation.
  size_t new_slot_size = Bucket::get_direct_map_size(raw_size);
  if (new_slot_size < kMinDirectMappedDownsize)
    return false;

  // bucket->slot_size is the current size of the allocation.
  size_t current_slot_size = slot_span->bucket->slot_size;
  char* char_ptr = static_cast<char*>(SlotSpan::ToPointer(slot_span));
  if (new_slot_size == current_slot_size) {
    // No need to move any memory around, but update size and cookie below.
  } else if (new_slot_size < current_slot_size) {
    size_t map_size = DirectMapExtent::FromSlotSpan(slot_span)->map_size;

    // Don't reallocate in-place if new size is less than 80 % of the full
    // map size, to avoid holding on to too much unused address space.
    if ((new_slot_size / SystemPageSize()) * 5 <
        (map_size / SystemPageSize()) * 4)
      return false;

    // Shrink by decommitting unneeded pages and making them inaccessible.
    size_t decommit_size = current_slot_size - new_slot_size;
    DecommitSystemPages(char_ptr + new_slot_size, decommit_size);
    SetSystemPagesAccess(char_ptr + new_slot_size, decommit_size,
                         PageInaccessible);
  } else if (new_slot_size <=
             DirectMapExtent::FromSlotSpan(slot_span)->map_size) {
    // Grow within the actually allocated memory. Just need to make the
    // pages accessible again.
    size_t recommit_slot_size_growth = new_slot_size - current_slot_size;
    SetSystemPagesAccess(char_ptr + current_slot_size,
                         recommit_slot_size_growth, PageReadWrite);
    RecommitSystemPages(char_ptr + current_slot_size,
                        recommit_slot_size_growth);

#if DCHECK_IS_ON()
    memset(char_ptr + current_slot_size, kUninitializedByte,
           recommit_slot_size_growth);
#endif
  } else {
    // We can't perform the realloc in-place.
    // TODO: support this too when possible.
    return false;
  }

#if DCHECK_IS_ON()
  // Write a new trailing cookie.
  if (allow_extras) {
    internal::PartitionCookieWriteValue(char_ptr + raw_size -
                                        internal::kCookieSize);
  }
#endif

  slot_span->SetRawSize(raw_size);
  slot_span->bucket->slot_size = new_slot_size;
  return true;
}

template <bool thread_safe>
void* PartitionRoot<thread_safe>::ReallocFlags(int flags,
                                               void* ptr,
                                               size_t new_size,
                                               const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(new_size, flags);
  void* result = realloc(ptr, new_size);
  PA_CHECK(result || flags & PartitionAllocReturnNull);
  return result;
#else
  bool no_hooks = flags & PartitionAllocNoHooks;
  if (UNLIKELY(!ptr)) {
    return no_hooks ? AllocFlagsNoHooks(flags, new_size)
                    : AllocFlags(flags, new_size, type_name);
  }

  if (UNLIKELY(!new_size)) {
    Free(ptr);
    return nullptr;
  }

  if (new_size > MaxDirectMapped()) {
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  bool overridden = false;
  size_t actual_old_size;
  if (UNLIKELY(!no_hooks && hooks_enabled)) {
    overridden = PartitionAllocHooks::ReallocOverrideHookIfEnabled(
        &actual_old_size, ptr);
  }
  if (LIKELY(!overridden)) {
    auto* slot_span = SlotSpan::FromPointer(
        internal::PartitionPointerAdjustSubtract(allow_extras, ptr));
    bool success = false;
    {
      internal::ScopedGuard<thread_safe> guard{lock_};
      // TODO(palmer): See if we can afford to make this a CHECK.
      PA_DCHECK(IsValidSlotSpan(slot_span));

      if (UNLIKELY(slot_span->bucket->is_direct_mapped())) {
        // We may be able to perform the realloc in place by changing the
        // accessibility of memory pages and, if reducing the size, decommitting
        // them.
        success = ReallocDirectMappedInPlace(slot_span, new_size);
      }
    }
    if (success) {
      if (UNLIKELY(!no_hooks && hooks_enabled)) {
        PartitionAllocHooks::ReallocObserverHookIfEnabled(ptr, ptr, new_size,
                                                          type_name);
      }
      return ptr;
    }

    const size_t actual_new_size = ActualSize(new_size);
    actual_old_size = GetSize(ptr);

    // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
    // new size is a significant percentage smaller. We could do the same if we
    // determine it is a win.
    if (actual_new_size == actual_old_size) {
      // Trying to allocate a block of size |new_size| would give us a block of
      // the same size as the one we've already got, so re-use the allocation
      // after updating statistics (and cookies, if present).
      if (slot_span->CanStoreRawSize()) {
        size_t new_raw_size =
            internal::PartitionSizeAdjustAdd(allow_extras, new_size);
        slot_span->SetRawSize(new_raw_size);
#if DCHECK_IS_ON()
        // Write a new trailing cookie only when it is possible to keep track
        // raw size (otherwise we wouldn't know where to look for it later).
        if (allow_extras) {
          internal::PartitionCookieWriteValue(static_cast<char*>(ptr) +
                                              new_size);
        }
#endif
      }
      return ptr;
    }
  }

  // This realloc cannot be resized in-place. Sadness.
  void* ret = no_hooks ? AllocFlagsNoHooks(flags, new_size)
                       : AllocFlags(flags, new_size, type_name);
  if (!ret) {
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  size_t copy_size = actual_old_size;
  if (new_size < copy_size)
    copy_size = new_size;

  memcpy(ret, ptr, copy_size);
  Free(ptr);
  return ret;
#endif
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::PurgeMemory(int flags) {
  {
    ScopedGuard guard{lock_};
    if (flags & PartitionPurgeDecommitEmptySlotSpans)
      DecommitEmptySlotSpans();
    if (flags & PartitionPurgeDiscardUnusedSystemPages) {
      for (size_t i = 0; i < kNumBuckets; ++i) {
        Bucket* bucket = &buckets[i];
        if (bucket->slot_size >= SystemPageSize())
          internal::PartitionPurgeBucket(bucket);
      }
    }
  }

  if (with_thread_cache)
    internal::ThreadCacheRegistry::Instance().PurgeAll();
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
  std::unique_ptr<uint32_t[]> direct_map_lengths = nullptr;
  if (!is_light_dump) {
    direct_map_lengths =
        std::unique_ptr<uint32_t[]>(new uint32_t[kMaxReportableDirectMaps]);
  }
  PartitionBucketMemoryStats bucket_stats[kNumBuckets];
  size_t num_direct_mapped_allocations = 0;
  PartitionMemoryStats stats = {0};

  // Collect data with the lock held, cannot allocate or call third-party code
  // below.
  {
    ScopedGuard guard{lock_};

    stats.total_mmapped_bytes =
        total_size_of_super_pages.load(std::memory_order_relaxed) +
        total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
    stats.total_committed_bytes =
        total_size_of_committed_pages.load(std::memory_order_relaxed);

    size_t direct_mapped_allocations_total_size = 0;
    for (size_t i = 0; i < kNumBuckets; ++i) {
      const Bucket* bucket = &buckets[i];
      // Don't report the pseudo buckets that the generic allocator sets up in
      // order to preserve a fast size->bucket map (see
      // PartitionRoot::Init() for details).
      if (!bucket->active_slot_spans_head)
        bucket_stats[i].is_valid = false;
      else
        internal::PartitionDumpBucketStats(&bucket_stats[i], bucket);
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
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

    stats.has_thread_cache = !is_light_dump && with_thread_cache;
    if (stats.has_thread_cache) {
      internal::ThreadCacheRegistry::Instance().DumpStats(
          true, &stats.current_thread_cache_stats);
      internal::ThreadCacheRegistry::Instance().DumpStats(
          false, &stats.all_thread_caches_stats);
    }
  }

  // Do not hold the lock when calling |dumper|, as it may allocate.
  if (!is_light_dump) {
    for (size_t i = 0; i < kNumBuckets; ++i) {
      if (bucket_stats[i].is_valid)
        dumper->PartitionsDumpBucketStats(partition_name, &bucket_stats[i]);
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
      mapped_stats.resident_bytes = size;
      dumper->PartitionsDumpBucketStats(partition_name, &mapped_stats);
    }
  }
  dumper->PartitionDumpTotals(partition_name, &stats);
}

template struct BASE_EXPORT PartitionRoot<internal::ThreadSafe>;
template struct BASE_EXPORT PartitionRoot<internal::NotThreadSafe>;

static_assert(sizeof(PartitionRoot<internal::ThreadSafe>) ==
                  sizeof(PartitionRoot<internal::NotThreadSafe>),
              "Layouts should match");
static_assert(offsetof(PartitionRoot<internal::ThreadSafe>, buckets) ==
                  offsetof(PartitionRoot<internal::NotThreadSafe>, buckets),
              "Layouts should match");
static_assert(offsetof(PartitionRoot<internal::ThreadSafe>, sentinel_bucket) ==
                  offsetof(PartitionRoot<internal::ThreadSafe>, buckets) +
                      kNumBuckets *
                          sizeof(PartitionRoot<internal::ThreadSafe>::Bucket),
              "sentinel_bucket must be just after the regular buckets.");

}  // namespace base
