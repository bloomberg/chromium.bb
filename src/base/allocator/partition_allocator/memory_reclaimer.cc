// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/memory_reclaimer.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/no_destructor.h"

// TODO(bikineev): Temporarily disable *Scan in MemoryReclaimer as it seems to
// cause significant jank.
#define PA_STARSCAN_ENABLE_STARSCAN_ON_RECLAIM 0

namespace base {

// static
PartitionAllocMemoryReclaimer* PartitionAllocMemoryReclaimer::Instance() {
  static NoDestructor<PartitionAllocMemoryReclaimer> instance;
  return instance.get();
}

void PartitionAllocMemoryReclaimer::RegisterPartition(
    PartitionRoot<>* partition) {
  internal::PartitionAutoLock lock(lock_);
  PA_DCHECK(partition);
  auto it_and_whether_inserted = partitions_.insert(partition);
  PA_DCHECK(it_and_whether_inserted.second);
}

void PartitionAllocMemoryReclaimer::UnregisterPartition(
    PartitionRoot<internal::ThreadSafe>* partition) {
  internal::PartitionAutoLock lock(lock_);
  PA_DCHECK(partition);
  size_t erased_count = partitions_.erase(partition);
  PA_DCHECK(erased_count == 1u);
}

PartitionAllocMemoryReclaimer::PartitionAllocMemoryReclaimer() = default;
PartitionAllocMemoryReclaimer::~PartitionAllocMemoryReclaimer() = default;

void PartitionAllocMemoryReclaimer::ReclaimAll() {
  constexpr int kFlags = PartitionPurgeDecommitEmptySlotSpans |
                         PartitionPurgeDiscardUnusedSystemPages |
                         PartitionPurgeAggressiveReclaim;
  Reclaim(kFlags);
}

void PartitionAllocMemoryReclaimer::ReclaimNormal() {
  constexpr int kFlags = PartitionPurgeDecommitEmptySlotSpans |
                         PartitionPurgeDiscardUnusedSystemPages;
  Reclaim(kFlags);
}

void PartitionAllocMemoryReclaimer::Reclaim(int flags) {
  internal::PartitionAutoLock lock(
      lock_);  // Has to protect from concurrent (Un)Register calls.

  // PCScan quarantines freed slots. Trigger the scan first to let it call
  // FreeNoHooksImmediate on slots that pass the quarantine.
  //
  // In turn, FreeNoHooksImmediate may add slots to thread cache. Purge it next
  // so that the slots are actually freed. (This is done synchronously only for
  // the current thread.)
  //
  // Lastly decommit empty slot spans and lastly try to discard unused pages at
  // the end of the remaining active slots.
#if PA_STARSCAN_ENABLE_STARSCAN_ON_RECLAIM
  {
    using PCScan = internal::PCScan;
    const auto invocation_mode = flags & PartitionPurgeAggressiveReclaim
                                     ? PCScan::InvocationMode::kForcedBlocking
                                     : PCScan::InvocationMode::kBlocking;
    PCScan::PerformScanIfNeeded(invocation_mode);
  }
#endif

#if defined(PA_THREAD_CACHE_SUPPORTED)
  // Don't completely empty the thread cache outside of low memory situations,
  // as there is periodic purge which makes sure that it doesn't take too much
  // space.
  if (flags & PartitionPurgeAggressiveReclaim)
    internal::ThreadCacheRegistry::Instance().PurgeAll();
#endif

  for (auto* partition : partitions_)
    partition->PurgeMemory(flags);
}

void PartitionAllocMemoryReclaimer::ResetForTesting() {
  internal::PartitionAutoLock lock(lock_);
  partitions_.clear();
}

}  // namespace base
