// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <string.h>

#include <cstdint>
#include <memory>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/dcheck_is_on.h"

namespace partition_alloc {

void PartitionAllocGlobalInit(OomFunction on_out_of_memory) {
  // This is from page_allocator_constants.h and doesn't really fit here, but
  // there isn't a centralized initialization function in page_allocator.cc, so
  // there's no good place in that file to do a STATIC_ASSERT_OR_PA_CHECK.
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::SystemPageSize() & internal::SystemPageOffsetMask()) == 0,
      "SystemPageSize() must be power of 2");

  // Two partition pages are used as guard / metadata page so make sure the
  // super page size is bigger.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::PartitionPageSize() * 4 <= internal::kSuperPageSize,
      "ok super page size");
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::kSuperPageSize & internal::SystemPageOffsetMask()) == 0,
      "ok super page multiple");
  // Four system pages gives us room to hack out a still-guard-paged piece
  // of metadata in the middle of a guard partition page.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::SystemPageSize() * 4 <= internal::PartitionPageSize(),
      "ok partition page size");
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::PartitionPageSize() & internal::SystemPageOffsetMask()) == 0,
      "ok partition page multiple");
  static_assert(sizeof(internal::PartitionPage<internal::ThreadSafe>) <=
                    internal::kPageMetadataSize,
                "PartitionPage should not be too big");
  STATIC_ASSERT_OR_PA_CHECK(
      internal::kPageMetadataSize * internal::NumPartitionPagesPerSuperPage() <=
          internal::SystemPageSize(),
      "page metadata fits in hole");

  // Limit to prevent callers accidentally overflowing an int size.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::MaxDirectMapped() <=
          (1UL << 31) + internal::DirectMapAllocationGranularity(),
      "maximum direct mapped allocation");

  // Check that some of our zanier calculations worked out as expected.
  static_assert(internal::kSmallestBucket == internal::kAlignment,
                "generic smallest bucket");
  static_assert(internal::kMaxBucketed == 917504, "generic max bucketed");
  STATIC_ASSERT_OR_PA_CHECK(
      internal::MaxSystemPagesPerRegularSlotSpan() <= 16,
      "System pages per slot span must be no greater than 16.");

  PA_DCHECK(on_out_of_memory);
  internal::g_oom_handling_function = on_out_of_memory;
}

void PartitionAllocGlobalUninitForTesting() {
  internal::PCScan::UninitForTesting();  // IN-TEST
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if defined(PA_HAS_64_BITS_POINTERS)
  internal::PartitionAddressSpace::UninitForTesting();
#else
  internal::AddressPoolManager::GetInstance()->ResetForTesting();
#endif  // defined(PA_HAS_64_BITS_POINTERS)
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  internal::g_oom_handling_function = nullptr;
}

namespace internal {

template <bool thread_safe>
PartitionAllocator<thread_safe>::~PartitionAllocator() {
  MemoryReclaimer::Instance()->UnregisterPartition(&partition_root_);
}

template <bool thread_safe>
void PartitionAllocator<thread_safe>::init(PartitionOptions opts) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(opts.thread_cache == PartitionOptions::ThreadCache::kDisabled)
      << "Cannot use a thread cache when PartitionAlloc is malloc().";
#endif
  partition_root_.Init(opts);
  MemoryReclaimer::Instance()->RegisterPartition(&partition_root_);
}

template PartitionAllocator<internal::ThreadSafe>::~PartitionAllocator();
template void PartitionAllocator<internal::ThreadSafe>::init(PartitionOptions);

#if (DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)) && \
    BUILDFLAG(USE_BACKUP_REF_PTR)
void CheckThatSlotOffsetIsZero(uintptr_t address) {
  // Add kPartitionPastAllocationAdjustment, because
  // PartitionAllocGetSlotStartInBRPPool will subtract it.
  PA_CHECK(PartitionAllocGetSlotStartInBRPPool(
               address + kPartitionPastAllocationAdjustment) == address);
}
#endif

}  // namespace internal

}  // namespace partition_alloc
