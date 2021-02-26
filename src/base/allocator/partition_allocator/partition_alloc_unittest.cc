// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif  // defined(OS_POSIX)

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

bool IsLargeMemoryDevice() {
  // Treat any device with 2GiB or more of physical memory as a "large memory
  // device". We check for slightly less than 2GiB so that devices with a small
  // amount of memory not accessible to the OS still count as "large".
  return base::SysInfo::AmountOfPhysicalMemory() >= 2040LL * 1024 * 1024;
}

bool SetAddressSpaceLimit() {
#if !defined(ARCH_CPU_64_BITS) || !defined(OS_POSIX)
  // 32 bits => address space is limited already.
  return true;
#elif defined(OS_POSIX) && !defined(OS_APPLE)
  // macOS will accept, but not enforce, |RLIMIT_AS| changes. See
  // https://crbug.com/435269 and rdar://17576114.
  //
  // Note: This number must be not less than 6 GB, because with
  // sanitizer_coverage_flags=edge, it reserves > 5 GB of address space. See
  // https://crbug.com/674665.
  const size_t kAddressSpaceLimit = static_cast<size_t>(6144) * 1024 * 1024;
  struct rlimit limit;
  if (getrlimit(RLIMIT_AS, &limit) != 0)
    return false;
  if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > kAddressSpaceLimit) {
    limit.rlim_cur = kAddressSpaceLimit;
    if (setrlimit(RLIMIT_AS, &limit) != 0)
      return false;
  }
  return true;
#else
  return false;
#endif
}

bool ClearAddressSpaceLimit() {
#if !defined(ARCH_CPU_64_BITS) || !defined(OS_POSIX)
  return true;
#elif defined(OS_POSIX)
  struct rlimit limit;
  if (getrlimit(RLIMIT_AS, &limit) != 0)
    return false;
  limit.rlim_cur = limit.rlim_max;
  if (setrlimit(RLIMIT_AS, &limit) != 0)
    return false;
  return true;
#else
  return false;
#endif
}

const size_t kTestSizes[] = {
    1,
    17,
    100,
    base::SystemPageSize(),
    base::SystemPageSize() + 1,
    base::PartitionRoot<
        base::internal::ThreadSafe>::Bucket::get_direct_map_size(100),
    1 << 20,
    1 << 21,
};
constexpr size_t kTestSizesCount = base::size(kTestSizes);

void AllocateRandomly(base::PartitionRoot<base::internal::ThreadSafe>* root,
                      size_t count,
                      int flags) {
  std::vector<void*> allocations(count, nullptr);
  for (size_t i = 0; i < count; ++i) {
    const size_t size = kTestSizes[base::RandGenerator(kTestSizesCount)];
    allocations[i] = root->AllocFlags(flags, size, nullptr);
    EXPECT_NE(nullptr, allocations[i]) << " size: " << size << " i: " << i;
  }

  for (size_t i = 0; i < count; ++i) {
    if (allocations[i])
      root->Free(allocations[i]);
  }
}

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

}  // namespace

namespace base {

// NOTE: Though this test actually excercises interfaces inside the ::base
// namespace, the unittest is inside the ::base::internal spaces because a
// portion of the test expectations require inspecting objects and behavior
// in the ::base::internal namespace. An alternate formulation would be to
// explicitly add using statements for each inspected type but this felt more
// readable.
namespace internal {

using SlotSpan = SlotSpanMetadata<ThreadSafe>;

const size_t kTestAllocSize = 16;
#if !DCHECK_IS_ON()
const size_t kPointerOffset = kInSlotTagBufferSize + kInSlotRefCountBufferSize;
const size_t kExtraAllocSize = kInSlotTagBufferSize + kInSlotRefCountBufferSize;
#else
const size_t kPointerOffset =
    kCookieSize + kInSlotTagBufferSize + kInSlotRefCountBufferSize;
const size_t kExtraAllocSize =
    kCookieSize * 2 + kInSlotTagBufferSize + kInSlotRefCountBufferSize;
#endif
const size_t kRealAllocSize = kTestAllocSize + kExtraAllocSize;

const char* type_name = nullptr;

class ScopedPageAllocation {
 public:
  ScopedPageAllocation(
      PartitionAllocator<base::internal::ThreadSafe>& allocator,
      base::CheckedNumeric<size_t> npages)
      : allocator_(allocator),
        npages_(npages),
        ptr_(reinterpret_cast<char*>(allocator_.root()->Alloc(
            (npages * SystemPageSize() - kExtraAllocSize).ValueOrDie(),
            type_name))) {}

  ~ScopedPageAllocation() { allocator_.root()->Free(ptr_); }

  void TouchAllPages() {
    memset(ptr_, 'A',
           ((npages_ * SystemPageSize()) - kExtraAllocSize).ValueOrDie());
  }

  void* PageAtIndex(size_t index) {
    return ptr_ - kPointerOffset + (SystemPageSize() * index);
  }

 private:
  PartitionAllocator<base::internal::ThreadSafe>& allocator_;
  const base::CheckedNumeric<size_t> npages_;
  char* ptr_;
};

class PartitionAllocTest : public testing::Test {
 protected:
  PartitionAllocTest() = default;

  ~PartitionAllocTest() override = default;

  void SetUp() override {
    scoped_feature_list.InitWithFeatures({features::kPartitionAllocGigaCage},
                                         {});
    PartitionAllocGlobalInit(HandleOOM);
    allocator.init({PartitionOptions::Alignment::kRegular});
    aligned_allocator.init({PartitionOptions::Alignment::kAlignedAlloc});
    test_bucket_index_ = SizeToIndex(kRealAllocSize);
  }

  size_t SizeToIndex(size_t size) {
    return PartitionRoot<base::internal::ThreadSafe>::SizeToBucketIndex(size);
  }

  void TearDown() override {
    allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                                  PartitionPurgeDiscardUnusedSystemPages);
    PartitionAllocGlobalUninitForTesting();
  }

  size_t GetNumPagesPerSlotSpan(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot<ThreadSafe>::Bucket* bucket =
        &allocator.root()->buckets[bucket_index];
    // TODO(tasak): make get_pages_per_slot_span() available at
    // partition_alloc_unittest.cc. Is it allowable to make the code from
    // partition_bucet.cc to partition_bucket.h?
    return (bucket->num_system_pages_per_slot_span +
            (NumSystemPagesPerPartitionPage() - 1)) /
           NumSystemPagesPerPartitionPage();
  }

  SlotSpan* GetFullSlotSpan(size_t size) {
    size_t real_size = size + kExtraAllocSize;
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot<ThreadSafe>::Bucket* bucket =
        &allocator.root()->buckets[bucket_index];
    size_t num_slots =
        (bucket->num_system_pages_per_slot_span * SystemPageSize()) /
        bucket->slot_size;
    void* first = nullptr;
    void* last = nullptr;
    size_t i;
    for (i = 0; i < num_slots; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      EXPECT_TRUE(ptr);
      if (!i)
        first = PartitionPointerAdjustSubtract(true, ptr);
      else if (i == num_slots - 1)
        last = PartitionPointerAdjustSubtract(true, ptr);
    }
    EXPECT_EQ(SlotSpan::FromPointer(first), SlotSpan::FromPointer(last));
    if (bucket->num_system_pages_per_slot_span ==
        NumSystemPagesPerPartitionPage())
      EXPECT_EQ(reinterpret_cast<size_t>(first) & PartitionPageBaseMask(),
                reinterpret_cast<size_t>(last) & PartitionPageBaseMask());
    EXPECT_EQ(num_slots,
              static_cast<size_t>(
                  bucket->active_slot_spans_head->num_allocated_slots));
    EXPECT_EQ(nullptr, bucket->active_slot_spans_head->freelist_head);
    EXPECT_TRUE(bucket->active_slot_spans_head);
    EXPECT_TRUE(bucket->active_slot_spans_head !=
                SlotSpan::get_sentinel_slot_span());
    return bucket->active_slot_spans_head;
  }

  void CycleFreeCache(size_t size) {
    for (size_t i = 0; i < kMaxFreeableSpans; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      auto* slot_span =
          SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
      auto* bucket = slot_span->bucket;
      EXPECT_EQ(1, bucket->active_slot_spans_head->num_allocated_slots);
      allocator.root()->Free(ptr);
      EXPECT_EQ(0, bucket->active_slot_spans_head->num_allocated_slots);
      EXPECT_NE(-1, bucket->active_slot_spans_head->empty_cache_index);
    }
  }

  enum ReturnNullTestMode {
    kPartitionAllocFlags,
    kPartitionReallocFlags,
    kPartitionRootTryRealloc,
  };

  void DoReturnNullTest(size_t alloc_size, ReturnNullTestMode mode) {
    // TODO(crbug.com/678782): Where necessary and possible, disable the
    // platform's OOM-killing behavior. OOM-killing makes this test flaky on
    // low-memory devices.
    if (!IsLargeMemoryDevice()) {
      LOG(WARNING)
          << "Skipping test on this device because of crbug.com/678782";
      LOG(FATAL) << "DoReturnNullTest";
    }

    ASSERT_TRUE(SetAddressSpaceLimit());

    // Work out the number of allocations for 6 GB of memory.
    const int num_allocations = (6 * 1024 * 1024) / (alloc_size / 1024);

    void** ptrs = reinterpret_cast<void**>(
        allocator.root()->Alloc(num_allocations * sizeof(void*), type_name));
    int i;

    for (i = 0; i < num_allocations; ++i) {
      switch (mode) {
        case kPartitionAllocFlags: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                                 alloc_size, type_name);
          break;
        }
        case kPartitionReallocFlags: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull, 1,
                                                 type_name);
          ptrs[i] = allocator.root()->ReallocFlags(
              PartitionAllocReturnNull, ptrs[i], alloc_size, type_name);
          break;
        }
        case kPartitionRootTryRealloc: {
          ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull, 1,
                                                 type_name);
          ptrs[i] =
              allocator.root()->TryRealloc(ptrs[i], alloc_size, type_name);
        }
      }

      if (!i)
        EXPECT_TRUE(ptrs[0]);
      if (!ptrs[i]) {
        ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                               alloc_size, type_name);
        EXPECT_FALSE(ptrs[i]);
        break;
      }
    }

    // We shouldn't succeed in allocating all 6 GB of memory. If we do, then
    // we're not actually testing anything here.
    EXPECT_LT(i, num_allocations);

    // Free, reallocate and free again each block we allocated. We do this to
    // check that freeing memory also works correctly after a failed allocation.
    for (--i; i >= 0; --i) {
      allocator.root()->Free(ptrs[i]);
      ptrs[i] = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                             alloc_size, type_name);
      EXPECT_TRUE(ptrs[i]);
      allocator.root()->Free(ptrs[i]);
    }

    allocator.root()->Free(ptrs);

    EXPECT_TRUE(ClearAddressSpaceLimit());
    LOG(FATAL) << "DoReturnNullTest";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  PartitionAllocator<base::internal::ThreadSafe> allocator;
  PartitionAllocator<base::internal::ThreadSafe> aligned_allocator;
  size_t test_bucket_index_;
};

class PartitionAllocDeathTest : public PartitionAllocTest {};

namespace {

void FreeFullSlotSpan(PartitionRoot<base::internal::ThreadSafe>* root,
                      SlotSpan* slot_span) {
  size_t size = slot_span->bucket->slot_size;
  size_t num_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      size;
  EXPECT_EQ(num_slots,
            static_cast<size_t>(std::abs(slot_span->num_allocated_slots)));
  char* ptr = reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span));
  size_t i;
  for (i = 0; i < num_slots; ++i) {
    root->Free(ptr + kPointerOffset);
    ptr += size;
  }
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
bool CheckPageInCore(void* ptr, bool in_core) {
  unsigned char ret = 0;
  EXPECT_EQ(0, mincore(ptr, SystemPageSize(), &ret));
  return in_core == (ret & 1);
}

#define CHECK_PAGE_IN_CORE(ptr, in_core) \
  EXPECT_TRUE(CheckPageInCore(ptr, in_core))
#else
#define CHECK_PAGE_IN_CORE(ptr, in_core) (void)(0)
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

class MockPartitionStatsDumper : public PartitionStatsDumper {
 public:
  MockPartitionStatsDumper()
      : total_resident_bytes(0),
        total_active_bytes(0),
        total_decommittable_bytes(0),
        total_discardable_bytes(0) {}

  void PartitionDumpTotals(const char* partition_name,
                           const PartitionMemoryStats* stats) override {
    EXPECT_GE(stats->total_mmapped_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_resident_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_active_bytes, stats->total_active_bytes);
    EXPECT_EQ(total_decommittable_bytes, stats->total_decommittable_bytes);
    EXPECT_EQ(total_discardable_bytes, stats->total_discardable_bytes);
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const PartitionBucketMemoryStats* stats) override {
    (void)partition_name;
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->bucket_slot_size & sizeof(void*));
    bucket_stats.push_back(*stats);
    total_resident_bytes += stats->resident_bytes;
    total_active_bytes += stats->active_bytes;
    total_decommittable_bytes += stats->decommittable_bytes;
    total_discardable_bytes += stats->discardable_bytes;
  }

  bool IsMemoryAllocationRecorded() {
    return total_resident_bytes != 0 && total_active_bytes != 0;
  }

  const PartitionBucketMemoryStats* GetBucketStats(size_t bucket_size) {
    for (size_t i = 0; i < bucket_stats.size(); ++i) {
      if (bucket_stats[i].bucket_slot_size == bucket_size)
        return &bucket_stats[i];
    }
    return nullptr;
  }

 private:
  size_t total_resident_bytes;
  size_t total_active_bytes;
  size_t total_decommittable_bytes;
  size_t total_discardable_bytes;

  std::vector<PartitionBucketMemoryStats> bucket_stats;
};

}  // namespace

// Check that the most basic of allocate / free pairs work.
TEST_F(PartitionAllocTest, Basic) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];
  auto* seed_slot_span = SlotSpan::get_sentinel_slot_span();

  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
  EXPECT_EQ(seed_slot_span, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, bucket->active_slot_spans_head->next_slot_span);

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(kPointerOffset,
            reinterpret_cast<size_t>(ptr) & PartitionPageOffsetMask());
  // Check that the offset appears to include a guard page.
  EXPECT_EQ(PartitionPageSize() + kPointerOffset + ReservedTagBitmapSize(),
            reinterpret_cast<size_t>(ptr) & kSuperPageOffsetMask);

  allocator.root()->Free(ptr);
  // Expect that the last active slot span gets noticed as empty but doesn't get
  // decommitted.
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
}

// Test multiple allocations, and freelist handling.
TEST_F(PartitionAllocTest, MultiAlloc) {
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  char* ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr1);
  EXPECT_TRUE(ptr2);
  ptrdiff_t diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);

  // Check that we re-use the just-freed slot.
  allocator.root()->Free(ptr2);
  ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr2);
  diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);
  allocator.root()->Free(ptr1);
  ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr1);
  diff = ptr2 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize), diff);

  char* ptr3 = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_TRUE(ptr3);
  diff = ptr3 - ptr1;
  EXPECT_EQ(static_cast<ptrdiff_t>(kRealAllocSize * 2), diff);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
}

// Test a bucket with multiple slot spans.
TEST_F(PartitionAllocTest, MultiSlotSpans) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span = GetFullSlotSpan(kTestAllocSize);
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span->next_slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  slot_span = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);

  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(SlotSpan::ToPointer(slot_span)) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(SlotSpan::ToPointer(slot_span2)) &
                kSuperPageBaseMask);

  // Fully free the non-current slot span. This will leave us with no current
  // active slot span because one is empty and the other is full.
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpanMetadata<ThreadSafe>::get_sentinel_slot_span(),
            bucket->active_slot_spans_head);

  // Allocate a new slot span, it should pull from the freelist.
  slot_span = GetFullSlotSpan(kTestAllocSize);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_EQ(slot_span, bucket->active_slot_spans_head);

  FreeFullSlotSpan(allocator.root(), slot_span);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_unprovisioned_slots);
  EXPECT_NE(-1, slot_span2->empty_cache_index);
}

// Test some finer aspects of internal slot span transitions.
TEST_F(PartitionAllocTest, SlotSpanTransitions) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span1->next_slot_span);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);

  // Bounce slot_span1 back into the non-full list then fill it up again.
  char* ptr =
      reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span1)) + kPointerOffset;
  allocator.root()->Free(ptr);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head->next_slot_span);

  // Allocating another slot span at this point should cause us to scan over
  // slot_span1 (which is both full and NOT our current slot span), and evict it
  // from the freelist. Older code had a O(n^2) condition due to failure to do
  // this.
  auto* slot_span3 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span3->next_slot_span);

  // Work out a pointer into slot_span2 and free it.
  ptr =
      reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span2)) + kPointerOffset;
  allocator.root()->Free(ptr);
  // Trying to allocate at this time should cause us to cycle around to
  // slot_span2 and find the recently freed slot.
  char* new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span3, slot_span2->next_slot_span);

  // Work out a pointer into slot_span1 and free it. This should pull the slot
  // span back into the list of available slot spans.
  ptr =
      reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span1)) + kPointerOffset;
  allocator.root()->Free(ptr);
  // This allocation should be satisfied by slot_span1.
  new_ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  EXPECT_EQ(ptr, new_ptr);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, slot_span1->next_slot_span);

  FreeFullSlotSpan(allocator.root(), slot_span3);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);

  // Allocating whilst in this state exposed a bug, so keep the test.
  ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(kTestAllocSize, type_name));
  allocator.root()->Free(ptr);
}

// Test some corner cases relating to slot span transitions in the internal
// free slot span list metadata bucket.
TEST_F(PartitionAllocTest, FreeSlotSpanListSlotSpanTransitions) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  size_t num_to_fill_free_list_slot_span =
      PartitionPageSize() / (sizeof(SlotSpan) + kExtraAllocSize);
  // The +1 is because we need to account for the fact that the current slot
  // span never gets thrown on the freelist.
  ++num_to_fill_free_list_slot_span;
  auto slot_spans =
      std::make_unique<SlotSpan*[]>(num_to_fill_free_list_slot_span);

  size_t i;
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);

  // Allocate / free in a different bucket size so we get control of a
  // different free slot span list. We need two slot spans because one will be
  // the last active slot span and not get freed.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize * 2);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize * 2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  FreeFullSlotSpan(allocator.root(), slot_span2);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
}

// Test a large series of allocations that cross more than one underlying
// super page.
TEST_F(PartitionAllocTest, MultiPageAllocs) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // 1 super page has 2 guard partition pages.
  size_t num_slot_spans_needed =
      (NumPartitionPagesPerSuperPage() - NumPartitionPagesPerTagBitmap() - 2) /
      num_pages_per_slot_span;

  // We need one more slot span in order to cross super page boundary.
  ++num_slot_spans_needed;

  EXPECT_GT(num_slot_spans_needed, 1u);
  auto slot_spans = std::make_unique<SlotSpan*[]>(num_slot_spans_needed);
  uintptr_t first_super_page_base = 0;
  size_t i;
  for (i = 0; i < num_slot_spans_needed; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
    void* storage_ptr = SlotSpan::ToPointer(slot_spans[i]);
    if (!i)
      first_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
    if (i == num_slot_spans_needed - 1) {
      uintptr_t second_super_page_base =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageBaseMask;
      uintptr_t second_super_page_offset =
          reinterpret_cast<uintptr_t>(storage_ptr) & kSuperPageOffsetMask;
      EXPECT_FALSE(second_super_page_base == first_super_page_base);
      // Check that we allocated a guard page for the second page.
      EXPECT_EQ(PartitionPageSize() + ReservedTagBitmapSize(),
                second_super_page_offset);
    }
  }
  for (i = 0; i < num_slot_spans_needed; ++i)
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
}

// Test the generic allocation functions that can handle arbitrary sizes and
// reallocing etc.
TEST_F(PartitionAllocTest, Alloc) {
  void* ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(kMaxBucketed + 1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  void* orig_ptr = ptr;
  char* char_ptr = static_cast<char*>(ptr);
  *char_ptr = 'A';

  // Change the size of the realloc, remaining inside the same bucket.
  void* new_ptr = allocator.root()->Realloc(ptr, 2, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr = allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_EQ(ptr, new_ptr);
  new_ptr = allocator.root()->Realloc(ptr, kSmallestBucket, type_name);
  EXPECT_EQ(ptr, new_ptr);

  // Change the size of the realloc, switching buckets.
  new_ptr = allocator.root()->Realloc(ptr, kSmallestBucket + 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  // Check that the realloc copied correctly.
  char* new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'A');
#if DCHECK_IS_ON()
  // Subtle: this checks for an old bug where we copied too much from the
  // source of the realloc. The condition can be detected by a trashing of
  // the uninitialized value in the space of the upsized allocation.
  EXPECT_EQ(kUninitializedByte,
            static_cast<unsigned char>(*(new_char_ptr + kSmallestBucket)));
#endif
  *new_char_ptr = 'B';
  // The realloc moved. To check that the old allocation was freed, we can
  // do an alloc of the old allocation size and check that the old allocation
  // address is at the head of the freelist and reused.
  void* reused_ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_EQ(reused_ptr, orig_ptr);
  allocator.root()->Free(reused_ptr);

  // Downsize the realloc.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'B');
  *new_char_ptr = 'C';

  // Upsize the realloc to outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed + 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'C');
  *new_char_ptr = 'D';

  // Upsize and downsize the realloc, remaining outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 10, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'D');
  *new_char_ptr = 'E';
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 2, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'E');
  *new_char_ptr = 'F';

  // Downsize the realloc to inside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, 1, type_name);
  EXPECT_NE(new_ptr, ptr);
  EXPECT_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'F');

  allocator.root()->Free(new_ptr);
}

// Test the generic allocation functions can handle some specific sizes of
// interest.
TEST_F(PartitionAllocTest, AllocSizes) {
  void* ptr = allocator.root()->Alloc(0, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  // PartitionPageSize() is interesting because it results in just one
  // allocation per page, which tripped up some corner cases.
  size_t size = PartitionPageSize() - kExtraAllocSize;
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);
  allocator.root()->Free(ptr);
  // Should be freeable at this point.
  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_NE(-1, slot_span->empty_cache_index);
  allocator.root()->Free(ptr2);

  size = (((PartitionPageSize() * kMaxPartitionPagesPerSlotSpan) -
           SystemPageSize()) /
          2) -
         kExtraAllocSize;
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);
  void* ptr3 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr3);
  void* ptr4 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr4);

  slot_span = SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
      PartitionPointerAdjustSubtract(true, ptr));
  auto* slot_span2 =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr3));
  EXPECT_NE(slot_span, slot_span2);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr2);
  // Should be freeable at this point.
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span->num_unprovisioned_slots);
  void* new_ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr3, new_ptr);
  new_ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr2, new_ptr);

  allocator.root()->Free(new_ptr);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);

#if DCHECK_IS_ON()
  // |SlotSpanMetadata::Free| must poison the slot's contents with |kFreedByte|.
  EXPECT_EQ(kFreedByte,
            *(reinterpret_cast<unsigned char*>(new_ptr) + (size - 1)));
#endif

  // Can we allocate a massive (512MB) size?
  // Allocate 512MB, but +1, to test for cookie writing alignment issues.
  // Test this only if the device has enough memory or it might fail due
  // to OOM.
  if (IsLargeMemoryDevice()) {
    ptr = allocator.root()->Alloc(512 * 1024 * 1024 + 1, type_name);
    allocator.root()->Free(ptr);
  }

  // Check a more reasonable, but still direct mapped, size.
  // Chop a system page and a byte off to test for rounding errors.
  size = 20 * 1024 * 1024;
  size -= SystemPageSize();
  size -= 1;
  ptr = allocator.root()->Alloc(size, type_name);
  char* char_ptr = reinterpret_cast<char*>(ptr);
  *(char_ptr + (size - 1)) = 'A';
  allocator.root()->Free(ptr);

  // Can we free null?
  allocator.root()->Free(nullptr);

  // Do we correctly get a null for a failed allocation?
  EXPECT_EQ(nullptr,
            allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                         3u * 1024 * 1024 * 1024, type_name));
}

// Test that we can fetch the real allocated size after an allocation.
TEST_F(PartitionAllocTest, AllocGetSizeAndOffset) {
  void* ptr;
  size_t requested_size, actual_size, predicted_size;

  // Allocate something small.
  requested_size = 511 - kExtraAllocSize;
  predicted_size = allocator.root()->ActualSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = allocator.root()->GetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_LT(requested_size, actual_size);
#if defined(PA_HAS_64_BITS_POINTERS)
  if (features::IsPartitionAllocGigaCageEnabled()) {
    for (size_t offset = 0; offset < requested_size; ++offset) {
      EXPECT_EQ(PartitionAllocGetSlotOffset(static_cast<char*>(ptr) + offset),
                offset);
    }
  }
#endif
  allocator.root()->Free(ptr);

  // Allocate a size that should be a perfect match for a bucket, because it
  // is an exact power of 2.
  requested_size = (256 * 1024) - kExtraAllocSize;
  predicted_size = allocator.root()->ActualSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = allocator.root()->GetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_EQ(requested_size, actual_size);
#if defined(PA_HAS_64_BITS_POINTERS)
  if (features::IsPartitionAllocGigaCageEnabled()) {
    for (size_t offset = 0; offset < requested_size; offset += 877) {
      EXPECT_EQ(PartitionAllocGetSlotOffset(static_cast<char*>(ptr) + offset),
                offset);
    }
  }
#endif
  allocator.root()->Free(ptr);

  // Allocate a size that is a system page smaller than a bucket. GetSize()
  // should return a larger size than we asked for now.
  size_t num = 64;
  while (num * SystemPageSize() >= 1024 * 1024) {
    num /= 2;
  }
  requested_size = num * SystemPageSize() - SystemPageSize() - kExtraAllocSize;
  predicted_size = allocator.root()->ActualSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = allocator.root()->GetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_EQ(requested_size + SystemPageSize(), actual_size);
#if defined(PA_HAS_64_BITS_POINTERS)
  if (features::IsPartitionAllocGigaCageEnabled()) {
    for (size_t offset = 0; offset < requested_size; offset += 4999) {
      EXPECT_EQ(PartitionAllocGetSlotOffset(static_cast<char*>(ptr) + offset),
                offset);
    }
  }
#endif

  // Allocate the maximum allowed bucketed size.
  requested_size = kMaxBucketed - kExtraAllocSize;
  predicted_size = allocator.root()->ActualSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  actual_size = allocator.root()->GetSize(ptr);
  EXPECT_EQ(predicted_size, actual_size);
  EXPECT_EQ(requested_size, actual_size);
#if defined(PA_HAS_64_BITS_POINTERS)
  if (features::IsPartitionAllocGigaCageEnabled()) {
    for (size_t offset = 0; offset < requested_size; offset += 4999) {
      EXPECT_EQ(PartitionAllocGetSlotOffset(static_cast<char*>(ptr) + offset),
                offset);
    }
  }
#endif

  // Check that we can write at the end of the reported size too.
  char* char_ptr = reinterpret_cast<char*>(ptr);
  *(char_ptr + (actual_size - 1)) = 'A';
  allocator.root()->Free(ptr);

  // Allocate something very large, and uneven.
  if (IsLargeMemoryDevice()) {
    requested_size = 512 * 1024 * 1024 - 1;
    predicted_size = allocator.root()->ActualSize(requested_size);
    ptr = allocator.root()->Alloc(requested_size, type_name);
    EXPECT_TRUE(ptr);
    actual_size = allocator.root()->GetSize(ptr);
    EXPECT_EQ(predicted_size, actual_size);
    EXPECT_LT(requested_size, actual_size);
    // Unlike above, don't test for PartitionAllocGetSlotOffset. Such large
    // allocations are direct-mapped, for which one can't easily obtain the
    // offset.
    allocator.root()->Free(ptr);
  }

  // Too large allocation.
  requested_size = MaxDirectMapped() + 1;
  predicted_size = allocator.root()->ActualSize(requested_size);
  EXPECT_EQ(requested_size, predicted_size);
}

#if defined(PA_HAS_64_BITS_POINTERS)
TEST_F(PartitionAllocTest, GetOffsetMultiplePages) {
  if (!features::IsPartitionAllocGigaCageEnabled())
    return;

  const size_t real_size = 80;
  const size_t requested_size = real_size - kExtraAllocSize;
  // Double check we don't end up with 0 or negative size.
  EXPECT_GT(requested_size, 0u);
  EXPECT_LE(requested_size, real_size);
  PartitionBucket<ThreadSafe>* bucket =
      allocator.root()->buckets + SizeToIndex(real_size);
  // Make sure the test is testing multiple partition pages case.
  EXPECT_GT(bucket->num_system_pages_per_slot_span,
            PartitionPageSize() / SystemPageSize());
  size_t num_slots =
      (bucket->num_system_pages_per_slot_span * SystemPageSize()) / real_size;
  std::vector<void*> ptrs;
  for (size_t i = 0; i < num_slots; ++i) {
    ptrs.push_back(allocator.root()->Alloc(requested_size, type_name));
  }
  for (size_t i = 0; i < num_slots; ++i) {
    char* ptr = static_cast<char*>(ptrs[i]);
    for (size_t offset = 0; offset < requested_size; offset += 13) {
      EXPECT_EQ(allocator.root()->GetSize(ptr), requested_size);
      EXPECT_EQ(PartitionAllocGetSlotOffset(ptr + offset), offset);
    }
    allocator.root()->Free(ptr);
  }
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

// Test the realloc() contract.
TEST_F(PartitionAllocTest, Realloc) {
  // realloc(0, size) should be equivalent to malloc().
  void* ptr = allocator.root()->Realloc(nullptr, kTestAllocSize, type_name);
  memset(ptr, 'A', kTestAllocSize);
  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  // realloc(ptr, 0) should be equivalent to free().
  void* ptr2 = allocator.root()->Realloc(ptr, 0, type_name);
  EXPECT_EQ(nullptr, ptr2);
  EXPECT_EQ(PartitionPointerAdjustSubtract(true, ptr),
            slot_span->freelist_head);

  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size_t size = SystemPageSize() - kExtraAllocSize;
  EXPECT_EQ(size, allocator.root()->ActualSize(size));
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr, size + 1, type_name);
  EXPECT_NE(ptr, ptr2);
  char* char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if DCHECK_IS_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation.
  ptr = allocator.root()->Realloc(ptr2, size - 1, type_name);
  EXPECT_NE(ptr2, ptr);
  char* char_ptr = static_cast<char*>(ptr);
  EXPECT_EQ('A', char_ptr[0]);
  EXPECT_EQ('A', char_ptr[size - 2]);
#if DCHECK_IS_ON()
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr[size - 1]));
#endif

  allocator.root()->Free(ptr);

  // Test that shrinking a direct mapped allocation happens in-place.
  size = kMaxBucketed + 16 * SystemPageSize();
  ptr = allocator.root()->Alloc(size, type_name);
  size_t actual_size = allocator.root()->GetSize(ptr);
  ptr2 = allocator.root()->Realloc(ptr, kMaxBucketed + 8 * SystemPageSize(),
                                   type_name);
  EXPECT_EQ(ptr, ptr2);
  EXPECT_EQ(actual_size - 8 * SystemPageSize(),
            allocator.root()->GetSize(ptr2));

  // Test that a previously in-place shrunk direct mapped allocation can be
  // expanded up again within its original size.
  ptr = allocator.root()->Realloc(ptr2, size - SystemPageSize(), type_name);
  EXPECT_EQ(ptr2, ptr);
  EXPECT_EQ(actual_size - SystemPageSize(), allocator.root()->GetSize(ptr));

  // Test that a direct mapped allocation is performed not in-place when the
  // new size is small enough.
  ptr2 = allocator.root()->Realloc(ptr, SystemPageSize(), type_name);
  EXPECT_NE(ptr, ptr2);

  allocator.root()->Free(ptr2);
}

// Tests the handing out of freelists for partial slot spans.
TEST_F(PartitionAllocTest, PartialPageFreelists) {
  size_t big_size = SystemPageSize() - kExtraAllocSize;
  size_t bucket_index = SizeToIndex(big_size + kExtraAllocSize);
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);

  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  size_t total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (big_size + kExtraAllocSize);
  EXPECT_EQ(4u, total_slots);
  // The freelist should have one entry, because we were able to exactly fit
  // one object slot and one freelist pointer (the null that the head points
  // to) into a system page.
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_EQ(3, slot_span->num_unprovisioned_slots);

  void* ptr2 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(2, slot_span->num_allocated_slots);
  EXPECT_EQ(2, slot_span->num_unprovisioned_slots);

  void* ptr3 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(3, slot_span->num_allocated_slots);
  EXPECT_EQ(1, slot_span->num_unprovisioned_slots);

  void* ptr4 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr4);
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(4, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span->num_unprovisioned_slots);

  void* ptr5 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr5);

  auto* slot_span2 =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr5));
  EXPECT_EQ(1, slot_span2->num_allocated_slots);

  // Churn things a little whilst there's a partial slot span freelist.
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(big_size, type_name);
  void* ptr6 = allocator.root()->Alloc(big_size, type_name);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr6);
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_NE(-1, slot_span2->empty_cache_index);
  EXPECT_TRUE(slot_span2->freelist_head);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);

  // And test a couple of sizes that do not cross SystemPageSize() with a single
  // allocation.
  size_t medium_size = (SystemPageSize() / 2) - kExtraAllocSize;
  bucket_index = SizeToIndex(medium_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(medium_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (medium_size + kExtraAllocSize);
  size_t first_slot_span_slots =
      SystemPageSize() / (medium_size + kExtraAllocSize);
  EXPECT_EQ(2u, first_slot_span_slots);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);

  size_t small_size = (SystemPageSize() / 4) - kExtraAllocSize;
  bucket_index = SizeToIndex(small_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (small_size + kExtraAllocSize);
  first_slot_span_slots = SystemPageSize() / (small_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  size_t very_small_size = (kExtraAllocSize <= 32) ? (32 - kExtraAllocSize) : 0;
  bucket_index = SizeToIndex(very_small_size + kExtraAllocSize);
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(very_small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (very_small_size + kExtraAllocSize);
  first_slot_span_slots =
      (SystemPageSize() + very_small_size + kExtraAllocSize - 1) /
      (very_small_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_EQ(0, slot_span->num_allocated_slots);

  // And try an allocation size (against the generic allocator) that is
  // larger than a system page.
  size_t page_and_a_half_size =
      (SystemPageSize() + (SystemPageSize() / 2)) - kExtraAllocSize;
  ptr = allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_TRUE(slot_span->freelist_head);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_and_a_half_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);

  // And then make sure than exactly the page size only faults one page.
  size_t page_size = SystemPageSize() - kExtraAllocSize;
  ptr = allocator.root()->Alloc(page_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  EXPECT_TRUE(slot_span->freelist_head);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_size + kExtraAllocSize);
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);
}

// Test some of the fragmentation-resistant properties of the allocator.
TEST_F(PartitionAllocTest, SlotSpanRefilling) {
  PartitionRoot<ThreadSafe>::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  // Grab two full slot spans and a non-full slot span.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_NE(slot_span1, bucket->active_slot_spans_head);
  EXPECT_NE(slot_span2, bucket->active_slot_spans_head);
  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(1, slot_span->num_allocated_slots);

  // Work out a pointer into slot_span2 and free it; and then slot_span1 and
  // free it.
  char* ptr2 =
      reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span1)) + kPointerOffset;
  allocator.root()->Free(ptr2);
  ptr2 =
      reinterpret_cast<char*>(SlotSpan::ToPointer(slot_span2)) + kPointerOffset;
  allocator.root()->Free(ptr2);

  // If we perform two allocations from the same bucket now, we expect to
  // refill both the nearly full slot spans.
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  (void)allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(1, slot_span->num_allocated_slots);

  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  allocator.root()->Free(ptr);
}

// Basic tests to ensure that allocations work for partial page buckets.
TEST_F(PartitionAllocTest, PartialPages) {
  // Find a size that is backed by a partial partition page.
  size_t size = sizeof(void*);
  size_t bucket_index;

  PartitionRoot<ThreadSafe>::Bucket* bucket = nullptr;
  while (size < 1000u) {
    bucket_index = SizeToIndex(size + kExtraAllocSize);
    bucket = &allocator.root()->buckets[bucket_index];
    if (bucket->num_system_pages_per_slot_span %
        NumSystemPagesPerPartitionPage())
      break;
    size += sizeof(void*);
  }
  EXPECT_LT(size, 1000u);

  auto* slot_span1 = GetFullSlotSpan(size);
  auto* slot_span2 = GetFullSlotSpan(size);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
}

// Test correct handling if our mapping collides with another.
TEST_F(PartitionAllocTest, MappingCollision) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // The -2 is because the first and last partition pages in a super page are
  // guard pages.
  size_t num_slot_span_needed =
      (NumPartitionPagesPerSuperPage() - NumPartitionPagesPerTagBitmap() - 2) /
      num_pages_per_slot_span;
  size_t num_partition_pages_needed =
      num_slot_span_needed * num_pages_per_slot_span;

  auto first_super_page_pages =
      std::make_unique<SlotSpan*[]>(num_partition_pages_needed);
  auto second_super_page_pages =
      std::make_unique<SlotSpan*[]>(num_partition_pages_needed);

  size_t i;
  for (i = 0; i < num_partition_pages_needed; ++i)
    first_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);

  char* page_base =
      reinterpret_cast<char*>(SlotSpan::ToPointer(first_super_page_pages[0]));
  EXPECT_EQ(PartitionPageSize() + ReservedTagBitmapSize(),
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= PartitionPageSize() - ReservedTagBitmapSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  void* map1 = AllocPages(
      page_base - PageAllocationGranularity(), PageAllocationGranularity(),
      PageAllocationGranularity(), PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  void* map2 = AllocPages(
      page_base + kSuperPageSize, PageAllocationGranularity(),
      PageAllocationGranularity(), PageInaccessible, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);

  for (i = 0; i < num_partition_pages_needed; ++i)
    second_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);

  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  page_base =
      reinterpret_cast<char*>(SlotSpan::ToPointer(second_super_page_pages[0]));
  EXPECT_EQ(PartitionPageSize() + ReservedTagBitmapSize(),
            reinterpret_cast<uintptr_t>(page_base) & kSuperPageOffsetMask);
  page_base -= PartitionPageSize() - ReservedTagBitmapSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  map1 = AllocPages(page_base - PageAllocationGranularity(),
                    PageAllocationGranularity(), PageAllocationGranularity(),
                    PageReadWrite, PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  map2 = AllocPages(page_base + kSuperPageSize, PageAllocationGranularity(),
                    PageAllocationGranularity(), PageReadWrite,
                    PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);
  EXPECT_TRUE(TrySetSystemPagesAccess(map1, PageAllocationGranularity(),
                                      PageInaccessible));
  EXPECT_TRUE(TrySetSystemPagesAccess(map2, PageAllocationGranularity(),
                                      PageInaccessible));

  auto* slot_span_in_third_super_page = GetFullSlotSpan(kTestAllocSize);
  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
                    SlotSpan::ToPointer(slot_span_in_third_super_page)) &
                    PartitionPageOffsetMask());

  // And make sure we really did get a page in a new superpage.
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                SlotSpan::ToPointer(first_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                SlotSpan::ToPointer(slot_span_in_third_super_page)) &
                kSuperPageBaseMask);
  EXPECT_NE(reinterpret_cast<uintptr_t>(
                SlotSpan::ToPointer(second_super_page_pages[0])) &
                kSuperPageBaseMask,
            reinterpret_cast<uintptr_t>(
                SlotSpan::ToPointer(slot_span_in_third_super_page)) &
                kSuperPageBaseMask);

  FreeFullSlotSpan(allocator.root(), slot_span_in_third_super_page);
  for (i = 0; i < num_partition_pages_needed; ++i) {
    FreeFullSlotSpan(allocator.root(), first_super_page_pages[i]);
    FreeFullSlotSpan(allocator.root(), second_super_page_pages[i]);
  }
}

// Tests that slot spans in the free slot span cache do get freed as
// appropriate.
TEST_F(PartitionAllocTest, FreeCache) {
  EXPECT_EQ(0U, allocator.root()->get_total_size_of_committed_pages());

  size_t big_size = 1000 - kExtraAllocSize;
  size_t bucket_index = SizeToIndex(big_size + kExtraAllocSize);
  PartitionBucket<base::internal::ThreadSafe>* bucket =
      &allocator.root()->buckets[bucket_index];

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);
  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr));
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(1, slot_span->num_allocated_slots);
  size_t expected_committed_size = PartitionPageSize();
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
  allocator.root()->Free(ptr);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_NE(-1, slot_span->empty_cache_index);
  EXPECT_TRUE(slot_span->freelist_head);

  CycleFreeCache(kTestAllocSize);

  // Flushing the cache should have really freed the unused slot spans.
  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_EQ(-1, slot_span->empty_cache_index);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  PartitionBucket<base::internal::ThreadSafe>* cycle_free_cache_bucket =
      &allocator.root()->buckets[test_bucket_index_];
  size_t expected_size =
      cycle_free_cache_bucket->num_system_pages_per_slot_span *
      SystemPageSize();
  EXPECT_EQ(expected_size,
            allocator.root()->get_total_size_of_committed_pages());

  // Check that an allocation works ok whilst in this state (a free'd slot span
  // as the active slot spans head).
  ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  allocator.root()->Free(ptr);

  // Also check that a slot span that is bouncing immediately between empty and
  // used does not get freed.
  for (size_t i = 0; i < kMaxFreeableSpans * 2; ++i) {
    ptr = allocator.root()->Alloc(big_size, type_name);
    EXPECT_TRUE(slot_span->freelist_head);
    allocator.root()->Free(ptr);
    EXPECT_TRUE(slot_span->freelist_head);
  }
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
}

// Tests for a bug we had with losing references to free slot spans.
TEST_F(PartitionAllocTest, LostFreeSlotSpansBug) {
  size_t size = PartitionPageSize() - kExtraAllocSize;

  void* ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);

  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span2 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr2));
  PartitionBucket<base::internal::ThreadSafe>* bucket = slot_span->bucket;

  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(-1, slot_span->num_allocated_slots);
  EXPECT_EQ(1, slot_span2->num_allocated_slots);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(0, slot_span->num_allocated_slots);
  EXPECT_EQ(0, slot_span2->num_allocated_slots);
  EXPECT_TRUE(slot_span->freelist_head);
  EXPECT_TRUE(slot_span2->freelist_head);

  CycleFreeCache(kTestAllocSize);

  EXPECT_FALSE(slot_span->freelist_head);
  EXPECT_FALSE(slot_span2->freelist_head);

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(
      SlotSpanMetadata<base::internal::ThreadSafe>::get_sentinel_slot_span(),
      bucket->active_slot_spans_head);

  // At this moment, we have two decommitted slot spans, on the empty list.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_EQ(
      SlotSpanMetadata<base::internal::ThreadSafe>::get_sentinel_slot_span(),
      bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);

  CycleFreeCache(kTestAllocSize);

  // We're now set up to trigger a historical bug by scanning over the active
  // slot spans list. The current code gets into a different state, but we'll
  // keep the test as being an interesting corner case.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_TRUE(bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);
}

// Death tests misbehave on Android, http://crbug.com/643760.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

// Unit tests that check if an allocation fails in "return null" mode,
// repeating it doesn't crash, and still returns null. The tests need to
// stress memory subsystem limits to do so, hence they try to allocate
// 6 GB of memory, each with a different per-allocation block sizes.
//
// On 64-bit systems we need to restrict the address space to force allocation
// failure, so these tests run only on POSIX systems that provide setrlimit(),
// and use it to limit address space to 6GB.
//
// Disable these tests on Android because, due to the allocation-heavy behavior,
// they tend to get OOM-killed rather than pass.
// TODO(https://crbug.com/779645): Fuchsia currently sets OS_POSIX, but does
// not provide a working setrlimit().
//
// Disable these test on Windows, since they run slower, so tend to timout and
// cause flake.
#if !defined(OS_WIN) &&            \
    (!defined(ARCH_CPU_64_BITS) || \
     (defined(OS_POSIX) && !(defined(OS_APPLE) || defined(OS_ANDROID))))

// The following four tests wrap a called function in an expect death statement
// to perform their test, because they are non-hermetic. Specifically they are
// going to attempt to exhaust the allocatable memory, which leaves the
// allocator in a bad global state.
// Performing them as death tests causes them to be forked into their own
// process, so they won't pollute other tests.
TEST_F(PartitionAllocDeathTest, RepeatedAllocReturnNullDirect) {
  // A direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionAllocFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc
TEST_F(PartitionAllocDeathTest, RepeatedReallocReturnNullDirect) {
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionReallocFlags),
               "DoReturnNullTest");
}

// Repeating above test with TryRealloc
TEST_F(PartitionAllocDeathTest, RepeatedTryReallocReturnNullDirect) {
  EXPECT_DEATH(DoReturnNullTest(32 * 1024 * 1024, kPartitionRootTryRealloc),
               "DoReturnNullTest");
}

// Test "return null" with a 512 kB block size.
TEST_F(PartitionAllocDeathTest, RepeatedAllocReturnNull) {
  // A single-slot but non-direct-mapped allocation size.
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionAllocFlags),
               "DoReturnNullTest");
}

// Repeating above test with Realloc.
TEST_F(PartitionAllocDeathTest, RepeatedReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionReallocFlags),
               "DoReturnNullTest");
}

// Repeating above test with TryRealloc.
TEST_F(PartitionAllocDeathTest, RepeatedTryReallocReturnNull) {
  EXPECT_DEATH(DoReturnNullTest(512 * 1024, kPartitionRootTryRealloc),
               "DoReturnNullTest");
}

#endif  // !defined(ARCH_CPU_64_BITS) || (defined(OS_POSIX) &&
        // !(defined(OS_APPLE) || defined(OS_ANDROID)))

// Make sure that malloc(-1) dies.
// In the past, we had an integer overflow that would alias malloc(-1) to
// malloc(0), which is not good.
TEST_F(PartitionAllocDeathTest, LargeAllocs) {
  // Largest alloc.
  EXPECT_DEATH(allocator.root()->Alloc(static_cast<size_t>(-1), type_name), "");
  // And the smallest allocation we expect to die.
  EXPECT_DEATH(allocator.root()->Alloc(MaxDirectMapped() + 1, type_name), "");
}

// TODO(glazunov): make BackupRefPtr compatible with the double-free detection.
#if !ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

// Check that our immediate double-free detection works.
TEST_F(PartitionAllocDeathTest, ImmediateDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

// Check that our refcount-based double-free detection works.
TEST_F(PartitionAllocDeathTest, RefcountDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr2);
  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  // This is not an immediate double-free so our immediate detection won't
  // fire. However, it does take the "refcount" of the to -1, which is illegal
  // and should be trapped.
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

#endif  // !ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

// Check that guard pages are present where expected.
TEST_F(PartitionAllocDeathTest, GuardPages) {
// PartitionAlloc adds PartitionPageSize() to the requested size
// (for metadata), and then rounds that size to PageAllocationGranularity().
// To be able to reliably write one past a direct allocation, choose a size
// that's
// a) larger than kMaxBucketed (to make the allocation direct)
// b) aligned at PageAllocationGranularity() boundaries after
//    PartitionPageSize() has been added to it.
// (On 32-bit, PartitionAlloc adds another SystemPageSize() to the
// allocation size before rounding, but there it marks the memory right
// after size as inaccessible, so it's fine to write 1 past the size we
// hand to PartitionAlloc and we don't need to worry about allocation
// granularities.)
#define ALIGN(N, A) (((N) + (A)-1) / (A) * (A))
  const size_t kSize = ALIGN(kMaxBucketed + 1 + PartitionPageSize(),
                             PageAllocationGranularity()) -
                       PartitionPageSize();
#undef ALIGN
  EXPECT_GT(kSize, kMaxBucketed)
      << "allocation not large enough for direct allocation";
  size_t size = kSize - kExtraAllocSize;
  void* ptr = allocator.root()->Alloc(size, type_name);

  EXPECT_TRUE(ptr);
  char* char_ptr = reinterpret_cast<char*>(ptr) - kPointerOffset;

  EXPECT_DEATH(*(char_ptr - 1) = 'A', "");
  EXPECT_DEATH(*(char_ptr + size + kExtraAllocSize) = 'A', "");

  allocator.root()->Free(ptr);
}

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that |PartitionDumpStats| and |PartitionDumpStats| run without
// crashing and return non-zero values when memory is allocated.
TEST_F(PartitionAllocTest, DumpMemoryStats) {
  {
    void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
    MockPartitionStatsDumper mock_stats_dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &mock_stats_dumper);
    EXPECT_TRUE(mock_stats_dumper.IsMemoryAllocationRecorded());
    allocator.root()->Free(ptr);
  }

  // This series of tests checks the active -> empty -> decommitted states.
  {
    {
      void* ptr = allocator.root()->Alloc(2048 - kExtraAllocSize, type_name);
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(2048u, stats->active_bytes);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(1u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
      allocator.root()->Free(ptr);
    }

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    // TODO(crbug.com/722911): Commenting this out causes this test to fail when
    // run singly (--gtest_filter=PartitionAllocTest.DumpMemoryStats), but not
    // when run with the others (--gtest_filter=PartitionAllocTest.*).
    CycleFreeCache(kTestAllocSize);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
  }

  // This test checks for correct empty slot span list accounting.
  {
    size_t size = PartitionPageSize() - kExtraAllocSize;
    void* ptr1 = allocator.root()->Alloc(size, type_name);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    allocator.root()->Free(ptr1);
    allocator.root()->Free(ptr2);

    CycleFreeCache(kTestAllocSize);

    ptr1 = allocator.root()->Alloc(size, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(PartitionPageSize());
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(PartitionPageSize(), stats->bucket_slot_size);
      EXPECT_EQ(PartitionPageSize(), stats->active_bytes);
      EXPECT_EQ(PartitionPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
    allocator.root()->Free(ptr1);
  }

  // This test checks for correct direct mapped accounting.
  {
    size_t size_smaller = kMaxBucketed + 1;
    size_t size_bigger = (kMaxBucketed * 2) + 1;
    size_t real_size_smaller =
        (size_smaller + SystemPageOffsetMask()) & SystemPageBaseMask();
    size_t real_size_bigger =
        (size_bigger + SystemPageOffsetMask()) & SystemPageBaseMask();
    void* ptr = allocator.root()->Alloc(size_smaller, type_name);
    void* ptr2 = allocator.root()->Alloc(size_bigger, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(real_size_smaller);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_smaller, stats->bucket_slot_size);
      EXPECT_EQ(real_size_smaller, stats->active_bytes);
      EXPECT_EQ(real_size_smaller, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);

      stats = dumper.GetBucketStats(real_size_bigger);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_bigger, stats->bucket_slot_size);
      EXPECT_EQ(real_size_bigger, stats->active_bytes);
      EXPECT_EQ(real_size_bigger, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr);

    // Whilst we're here, allocate again and free with different ordering to
    // give a workout to our linked list code.
    ptr = allocator.root()->Alloc(size_smaller, type_name);
    ptr2 = allocator.root()->Alloc(size_bigger, type_name);
    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
  }

  // This test checks large-but-not-quite-direct allocations.
  {
    const size_t requested_size = 16 * SystemPageSize();
    void* ptr = allocator.root()->Alloc(requested_size + 1, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + 1 + kExtraAllocSize, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(slot_size, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    void* ptr2 = allocator.root()->Alloc(requested_size + SystemPageSize() + 1,
                                         type_name);
    EXPECT_EQ(ptr, ptr2);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size =
          requested_size + (requested_size / kNumBucketsPerOrder);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + SystemPageSize() + 1 + kExtraAllocSize,
                stats->active_bytes);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
  }
}

// Tests the API to purge freeable memory.
TEST_F(PartitionAllocTest, Purge) {
  char* ptr = reinterpret_cast<char*>(
      allocator.root()->Alloc(2048 - kExtraAllocSize, type_name));
  allocator.root()->Free(ptr);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
  }
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(0u, stats->resident_bytes);
  }
  // Calling purge again here is a good way of testing we didn't mess up the
  // state of the free cache ring.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);

  char* big_ptr =
      reinterpret_cast<char*>(allocator.root()->Alloc(256 * 1024, type_name));
  allocator.root()->Free(big_ptr);
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);

  CHECK_PAGE_IN_CORE(ptr - kPointerOffset, false);
  CHECK_PAGE_IN_CORE(big_ptr - kPointerOffset, false);
}

// Tests that we prefer to allocate into a non-empty partition page over an
// empty one. This is an important aspect of minimizing memory usage for some
// allocation sizes, particularly larger ones.
TEST_F(PartitionAllocTest, PreferActiveOverEmpty) {
  size_t size = (SystemPageSize() * 2) - kExtraAllocSize;
  // Allocate 3 full slot spans worth of 8192-byte allocations.
  // Each slot span for this size is 16384 bytes, or 1 partition page and 2
  // slots.
  void* ptr1 = allocator.root()->Alloc(size, type_name);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  void* ptr3 = allocator.root()->Alloc(size, type_name);
  void* ptr4 = allocator.root()->Alloc(size, type_name);
  void* ptr5 = allocator.root()->Alloc(size, type_name);
  void* ptr6 = allocator.root()->Alloc(size, type_name);

  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span1 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr1));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span2 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr3));
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span3 =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr6));
  EXPECT_NE(slot_span1, slot_span2);
  EXPECT_NE(slot_span2, slot_span3);
  PartitionBucket<base::internal::ThreadSafe>* bucket = slot_span1->bucket;
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  // Free up the 2nd slot in each slot span.
  // This leaves the active list containing 3 slot spans, each with 1 used and 1
  // free slot. The active slot span will be the one containing ptr1.
  allocator.root()->Free(ptr6);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr2);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the middle slot span in the active list.
  allocator.root()->Free(ptr3);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the first slot span in the active list -- also the current slot span.
  allocator.root()->Free(ptr1);

  // A good choice here is to re-fill the third slot span since the first two
  // are empty. We used to fail that.
  void* ptr7 = allocator.root()->Alloc(size, type_name);
  EXPECT_EQ(ptr6, ptr7);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr7);
}

// Tests the API to purge discardable memory.
TEST_F(PartitionAllocTest, PurgeDiscardableSecondPage) {
  // Free the second of two 4096 byte allocations and then purge.
  void* ptr1 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  char* ptr2 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  allocator.root()->Free(ptr2);
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr1));
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, false);
  EXPECT_EQ(3u, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr1);
}

TEST_F(PartitionAllocTest, PurgeDiscardableFirstPage) {
  // Free the first of two 4096 byte allocations and then purge.
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
    EXPECT_EQ(0u, stats->discardable_bytes);
#else
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, false);

  allocator.root()->Free(ptr2);
}

TEST_F(PartitionAllocTest, PurgeDiscardableNonPageSizedAlloc) {
  const size_t requested_size = 2.25 * SystemPageSize();
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(requested_size - kExtraAllocSize, type_name);
  memset(ptr1, 'A', requested_size - kExtraAllocSize);
  memset(ptr2, 'A', requested_size - kExtraAllocSize);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(requested_size);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(requested_size * 2, stats->active_bytes);
    EXPECT_EQ(9 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), true);

  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
}

TEST_F(PartitionAllocTest, PurgeDiscardableManyPages) {
  // On systems with large pages, use less pages because:
  // 1) There must be a bucket for kFirstAllocPages * SystemPageSize(), and
  // 2) On low-end systems, using too many large pages can OOM during the test
  const bool kHasLargePages = SystemPageSize() > 4096;
  const size_t kFirstAllocPages = kHasLargePages ? 32 : 64;
  const size_t kSecondAllocPages = kHasLargePages ? 31 : 61;

  // Detect case (1) from above.
  DCHECK_LT(kFirstAllocPages * SystemPageSize(), 1UL << kMaxBucketedOrder);

  const size_t kDeltaPages = kFirstAllocPages - kSecondAllocPages;

  {
    ScopedPageAllocation p(allocator, kFirstAllocPages);
    p.TouchAllPages();
  }

  ScopedPageAllocation p(allocator, kSecondAllocPages);

  MockPartitionStatsDumper dumper;
  allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                              &dumper);
  EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

  const PartitionBucketMemoryStats* stats =
      dumper.GetBucketStats(kFirstAllocPages * SystemPageSize());
  EXPECT_TRUE(stats);
  EXPECT_TRUE(stats->is_valid);
  EXPECT_EQ(0u, stats->decommittable_bytes);
  EXPECT_EQ(kDeltaPages * SystemPageSize(), stats->discardable_bytes);
  EXPECT_EQ(kSecondAllocPages * SystemPageSize(), stats->active_bytes);
  EXPECT_EQ(kFirstAllocPages * SystemPageSize(), stats->resident_bytes);

  for (size_t i = 0; i < kFirstAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);

  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);

  for (size_t i = 0; i < kSecondAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);
  for (size_t i = kSecondAllocPages; i < kFirstAllocPages; i++)
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), false);
}

TEST_F(PartitionAllocTest, PurgeDiscardableWithFreeListRewrite) {
  // This sub-test tests truncation of the provisioned slots in a trickier
  // case where the freelist is rewritten.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr1));
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr1);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if defined(OS_WIN)
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#else
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  EXPECT_EQ(1u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  // Let's check we didn't brick the freelist.
  void* ptr1b =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  EXPECT_EQ(ptr1, ptr1b);
  void* ptr2b =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  EXPECT_EQ(ptr2, ptr2b);
  EXPECT_FALSE(slot_span->freelist_head);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
}

TEST_F(PartitionAllocTest, PurgeDiscardableDoubleTruncateFreeList) {
  // This sub-test is similar, but tests a double-truncation.
  allocator.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans);
  char* ptr1 = reinterpret_cast<char*>(
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name));
  void* ptr2 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr3 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  void* ptr4 =
      allocator.root()->Alloc(SystemPageSize() - kExtraAllocSize, type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  SlotSpanMetadata<base::internal::ThreadSafe>* slot_span =
      SlotSpanMetadata<base::internal::ThreadSafe>::FromPointer(
          PartitionPointerAdjustSubtract(true, ptr1));
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr3);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PartitionPurgeDiscardUnusedSystemPages);
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  EXPECT_FALSE(slot_span->freelist_head);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
}

TEST_F(PartitionAllocTest, ReallocMovesCookies) {
  // Resize so as to be sure to hit a "resize in place" case, and ensure that
  // use of the entire result is compatible with the debug mode's cookies, even
  // when the bucket size is large enough to span more than one partition page
  // and we can track the "raw" size. See https://crbug.com/709271
  static const size_t kSize =
      base::MaxSystemPagesPerSlotSpan() * base::SystemPageSize();
  void* ptr = allocator.root()->Alloc(kSize + 1, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 1);
  ptr = allocator.root()->Realloc(ptr, kSize + 2, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 2);
  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, SmallReallocDoesNotMoveTrailingCookie) {
  // For crbug.com/781473
  static constexpr size_t kSize = 264;
  void* ptr = allocator.root()->Alloc(kSize, type_name);
  EXPECT_TRUE(ptr);

  ptr = allocator.root()->Realloc(ptr, kSize + 16, type_name);
  EXPECT_TRUE(ptr);

  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, ZeroFill) {
  constexpr static size_t kAllZerosSentinel =
      std::numeric_limits<size_t>::max();
  for (size_t size : kTestSizes) {
    char* p = static_cast<char*>(
        allocator.root()->AllocFlags(PartitionAllocZeroFill, size, nullptr));
    size_t non_zero_position = kAllZerosSentinel;
    for (size_t i = 0; i < size; ++i) {
      if (0 != p[i]) {
        non_zero_position = i;
        break;
      }
    }
    EXPECT_EQ(kAllZerosSentinel, non_zero_position)
        << "test allocation size: " << size;
    allocator.root()->Free(p);
  }

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);
    AllocateRandomly(allocator.root(), 250, PartitionAllocZeroFill);
  }
}

TEST_F(PartitionAllocTest, Bug_897585) {
  // Need sizes big enough to be direct mapped and a delta small enough to
  // allow re-use of the slot span when cookied. These numbers fall out of the
  // test case in the indicated bug.
  size_t kInitialSize = 983040;
  size_t kDesiredSize = 983100;
  void* ptr = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                           kInitialSize, nullptr);
  ASSERT_NE(nullptr, ptr);
  ptr = allocator.root()->ReallocFlags(PartitionAllocReturnNull, ptr,
                                       kDesiredSize, nullptr);
  ASSERT_NE(nullptr, ptr);
  memset(ptr, 0xbd, kDesiredSize);
  allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, OverrideHooks) {
  constexpr size_t kOverriddenSize = 1234;
  constexpr const char* kOverriddenType = "Overridden type";
  constexpr unsigned char kOverriddenChar = 'A';

  // Marked static so that we can use them in non-capturing lambdas below.
  // (Non-capturing lambdas convert directly to function pointers.)
  static volatile bool free_called = false;
  static void* overridden_allocation = malloc(kOverriddenSize);
  memset(overridden_allocation, kOverriddenChar, kOverriddenSize);

  PartitionAllocHooks::SetOverrideHooks(
      [](void** out, int flags, size_t size, const char* type_name) -> bool {
        if (size == kOverriddenSize && type_name == kOverriddenType) {
          *out = overridden_allocation;
          return true;
        }
        return false;
      },
      [](void* address) -> bool {
        if (address == overridden_allocation) {
          free_called = true;
          return true;
        }
        return false;
      },
      [](size_t* out, void* address) -> bool {
        if (address == overridden_allocation) {
          *out = kOverriddenSize;
          return true;
        }
        return false;
      });

  void* ptr = allocator.root()->AllocFlags(PartitionAllocReturnNull,
                                           kOverriddenSize, kOverriddenType);
  ASSERT_EQ(ptr, overridden_allocation);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(free_called);

  // overridden_allocation has not actually been freed so we can now immediately
  // realloc it.
  free_called = false;
  ptr =
      allocator.root()->ReallocFlags(PartitionAllocReturnNull, ptr, 1, nullptr);
  ASSERT_NE(ptr, nullptr);
  EXPECT_NE(ptr, overridden_allocation);
  EXPECT_TRUE(free_called);
  EXPECT_EQ(*(char*)ptr, kOverriddenChar);
  allocator.root()->Free(ptr);

  PartitionAllocHooks::SetOverrideHooks(nullptr, nullptr, nullptr);
  free(overridden_allocation);
}

TEST_F(PartitionAllocTest, Alignment) {
  std::vector<void*> allocated_ptrs;

  for (size_t size = 1; size <= base::SystemPageSize(); size <<= 1) {
    // All allocations which are not direct-mapped occupy contiguous slots of a
    // span, starting on a page boundary. This means that allocations are first
    // rounded up to the nearest bucket size, then have an address of the form:
    //
    // (page-aligned address) + i * bucket_size.

    // All powers of two are bucket sizes, meaning that all power of two
    // allocations smaller than a page will be aligned on the allocation size.
    size_t expected_alignment = size;
#if DCHECK_IS_ON()
    // When DCHECK_IS_ON(), a kCookieSize cookie is added on both sides before
    // rounding up the allocation size. The returned pointer points after the
    // cookie.
    expected_alignment = std::min(expected_alignment, kCookieSize);
#endif
#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR
    // When ENABLE_TAG_FOR_CHECKED_PTR2, a kInSlotTagBufferSize is added before
    // rounding up the allocation size. The returned pointer points after the
    // partition tag.
    expected_alignment = std::min(
        {expected_alignment, kInSlotTagBufferSize + kInSlotRefCountBufferSize});
#endif
    for (int index = 0; index < 3; index++) {
      void* ptr = allocator.root()->Alloc(size, "");
      allocated_ptrs.push_back(ptr);
      EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) % expected_alignment)
          << index << "-th allocation of size = " << size;
    }
  }

  for (void* ptr : allocated_ptrs)
    allocator.root()->Free(ptr);
}

TEST_F(PartitionAllocTest, FundamentalAlignment) {
  // See the test above for details. Essentially, checking the bucket size is
  // sufficient to ensure that alignment will always be respected, as long as
  // the fundamental alignment is <= 16 bytes.
  size_t fundamental_alignment = base::kAlignment;
  for (size_t size = 0; size < base::SystemPageSize(); size++) {
    // Allocate several pointers, as the first one in use in a size class will
    // be aligned on a page boundary.
    void* ptr = allocator.root()->Alloc(size, "");
    void* ptr2 = allocator.root()->Alloc(size, "");
    void* ptr3 = allocator.root()->Alloc(size, "");

    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % fundamental_alignment,
              static_cast<uintptr_t>(0));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % fundamental_alignment,
              static_cast<uintptr_t>(0));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr3) % fundamental_alignment,
              static_cast<uintptr_t>(0));

    EXPECT_EQ(allocator.root()->GetSize(ptr) % fundamental_alignment,
              static_cast<uintptr_t>(0));

    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr3);
  }
}

TEST_F(PartitionAllocTest, AlignedAllocations) {
  size_t alloc_sizes[] = {1, 10, 100, 1000, 100000, 1000000};
  size_t alignemnts[] = {8, 16, 32, 64, 1024, 4096};

  for (size_t alloc_size : alloc_sizes) {
    for (size_t alignment : alignemnts) {
      void* ptr =
          aligned_allocator.root()->AlignedAllocFlags(0, alignment, alloc_size);
      ASSERT_TRUE(ptr);
      EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0ull);
      allocator.root()->Free(ptr);
    }
  }
}

#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_MTE_CHECKED_PTR || \
    ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR

TEST_F(PartitionAllocTest, TagBasic) {
  size_t alloc_size = 64 - kExtraAllocSize;
  void* ptr1 = allocator.root()->Alloc(alloc_size, type_name);
  void* ptr2 = allocator.root()->Alloc(alloc_size, type_name);
  void* ptr3 = allocator.root()->Alloc(alloc_size, type_name);
  EXPECT_TRUE(ptr1);
  EXPECT_TRUE(ptr2);
  EXPECT_TRUE(ptr3);

  auto* slot_span =
      SlotSpan::FromPointer(PartitionPointerAdjustSubtract(true, ptr1));
  EXPECT_TRUE(slot_span);

  char* char_ptr1 = reinterpret_cast<char*>(ptr1);
  char* char_ptr2 = reinterpret_cast<char*>(ptr2);
  char* char_ptr3 = reinterpret_cast<char*>(ptr3);
  EXPECT_LT(kTestAllocSize, slot_span->bucket->slot_size);
  EXPECT_EQ(char_ptr1 + slot_span->bucket->slot_size, char_ptr2);
  EXPECT_EQ(char_ptr2 + slot_span->bucket->slot_size, char_ptr3);

#if !ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR
  constexpr PartitionTag kTag1 = static_cast<PartitionTag>(0xBADA);
  constexpr PartitionTag kTag2 = static_cast<PartitionTag>(0xDB8A);
  constexpr PartitionTag kTag3 = static_cast<PartitionTag>(0xA3C4);
#else
  // The in-memory tag will always be kFixedTagValue no matter what we set.
  constexpr PartitionTag kTag1 = static_cast<PartitionTag>(kFixedTagValue);
  constexpr PartitionTag kTag2 = static_cast<PartitionTag>(kFixedTagValue);
  constexpr PartitionTag kTag3 = static_cast<PartitionTag>(kFixedTagValue);
#endif
  PartitionTagSetValue(ptr1, slot_span->bucket->slot_size, kTag1);
  PartitionTagSetValue(ptr2, slot_span->bucket->slot_size, kTag2);
  PartitionTagSetValue(ptr3, slot_span->bucket->slot_size, kTag3);

  memset(ptr1, 0, alloc_size);
  memset(ptr2, 0, alloc_size);
  memset(ptr3, 0, alloc_size);

  EXPECT_EQ(kTag1, PartitionTagGetValue(ptr1));
  EXPECT_EQ(kTag2, PartitionTagGetValue(ptr2));
  EXPECT_EQ(kTag3, PartitionTagGetValue(ptr3));

  EXPECT_TRUE(!memchr(ptr1, static_cast<uint8_t>(kTag1), alloc_size));
  EXPECT_TRUE(!memchr(ptr2, static_cast<uint8_t>(kTag2), alloc_size));
  if (sizeof(PartitionTag) > 1) {
    EXPECT_TRUE(!memchr(ptr1, static_cast<uint8_t>(kTag1 >> 8), alloc_size));
    EXPECT_TRUE(!memchr(ptr2, static_cast<uint8_t>(kTag2 >> 8), alloc_size));
  }

  allocator.root()->Free(ptr1);
  EXPECT_EQ(kTag2, PartitionTagGetValue(ptr2));

  size_t request_size = slot_span->bucket->slot_size - kExtraAllocSize;
  void* new_ptr2 = allocator.root()->Realloc(ptr2, request_size, type_name);
  EXPECT_EQ(ptr2, new_ptr2);
  EXPECT_EQ(kTag3, PartitionTagGetValue(ptr3));

  // Add 1B to ensure the object is rellocated to a larger slot.
  request_size = slot_span->bucket->slot_size - kExtraAllocSize + 1;
  new_ptr2 = allocator.root()->Realloc(ptr2, request_size, type_name);
  EXPECT_TRUE(new_ptr2);
  EXPECT_NE(ptr2, new_ptr2);

  allocator.root()->Free(new_ptr2);

  EXPECT_EQ(kTag3, PartitionTagGetValue(ptr3));
  allocator.root()->Free(ptr3);
}

#endif

// Test that the optimized `GetSlotOffset` implementation produces valid
// results.
TEST_F(PartitionAllocTest, OptimizedGetSlotOffset) {
  auto* current_bucket = allocator.root()->buckets;

  for (size_t i = 0; i < kNumBuckets; ++i, ++current_bucket) {
    for (size_t offset = 0; offset <= kMaxBucketed; offset += 4999) {
      EXPECT_EQ(offset % current_bucket->slot_size,
                current_bucket->GetSlotOffset(offset));
    }
  }
}

// Test that the optimized `GetSlotNumber` implementation produces valid
// results.
TEST_F(PartitionAllocTest, OptimizedGetSlotNumber) {
  for (auto& bucket : allocator.root()->buckets) {
    for (size_t slot = 0, offset = bucket.slot_size / 2;
         slot < bucket.get_slots_per_span();
         ++slot, offset += bucket.slot_size) {
      EXPECT_EQ(slot, bucket.GetSlotNumber(offset));
    }
  }
}

TEST_F(PartitionAllocTest, GetUsableSize) {
  size_t delta = SystemPageSize() + 1;
  for (size_t size = 1; size <= kMinDirectMappedDownsize; size += delta) {
    void* ptr = allocator.root()->Alloc(size, "");
    EXPECT_TRUE(ptr);
    size_t usable_size = PartitionRoot<ThreadSafe>::GetUsableSize(ptr);
    EXPECT_LE(size, usable_size);
    memset(ptr, 0xDE, usable_size);
    // Should not crash when free the ptr.
    allocator.root()->Free(ptr);
  }
}

#if ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

TEST_F(PartitionAllocTest, RefCountBasic) {
  constexpr uint64_t kCookie = 0x1234567890ABCDEF;

  size_t alloc_size = 64 - kExtraAllocSize;
  uint64_t* ptr1 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr1);

  *ptr1 = kCookie;

  auto* ref_count = PartitionRefCountPointer(ptr1);

  ref_count->AddRef();
  ref_count->Release();
  EXPECT_TRUE(ref_count->HasOneRef());
  EXPECT_EQ(*ptr1, kCookie);

  ref_count->AddRef();
  EXPECT_FALSE(ref_count->HasOneRef());

  allocator.root()->Free(ptr1);
  EXPECT_NE(*ptr1, kCookie);

  // The allocator should not reuse the original slot since its reference count
  // doesn't equal zero.
  uint64_t* ptr2 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_NE(ptr1, ptr2);
  allocator.root()->Free(ptr2);

  // When the last reference is released, the slot should become reusable.
  ref_count->Release();
  uint64_t* ptr3 = reinterpret_cast<uint64_t*>(
      allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_EQ(ptr1, ptr3);
  allocator.root()->Free(ptr3);
}

#endif

}  // namespace internal
}  // namespace base

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
