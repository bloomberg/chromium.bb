// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/memory_reclaimer.h"

#include <memory>
#include <utility>

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
#include "base/allocator/partition_allocator/thread_cache.h"
#endif

// Otherwise, PartitionAlloc doesn't allocate any memory, and the tests are
// meaningless.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace base {

namespace {

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

}  // namespace

class PartitionAllocMemoryReclaimerTest : public ::testing::Test {
 public:
  PartitionAllocMemoryReclaimerTest() = default;

 protected:
  void SetUp() override {
    PartitionAllocGlobalInit(HandleOOM);
    PartitionAllocMemoryReclaimer::Instance()->ResetForTesting();
    allocator_ = std::make_unique<PartitionAllocator>();
    allocator_->init({
        PartitionOptions::AlignedAlloc::kDisallowed,
        PartitionOptions::ThreadCache::kDisabled,
        PartitionOptions::Quarantine::kAllowed,
        PartitionOptions::Cookie::kAllowed,
        PartitionOptions::BackupRefPtr::kDisabled,
        PartitionOptions::UseConfigurablePool::kNo,
    });
    allocator_->root()->UncapEmptySlotSpanMemoryForTesting();
  }

  void TearDown() override {
    allocator_ = nullptr;
    PartitionAllocMemoryReclaimer::Instance()->ResetForTesting();
    PartitionAllocGlobalUninitForTesting();
  }

  void Reclaim() { PartitionAllocMemoryReclaimer::Instance()->ReclaimNormal(); }

  void AllocateAndFree() {
    void* data = allocator_->root()->Alloc(1, "");
    allocator_->root()->Free(data);
  }

  std::unique_ptr<PartitionAllocator> allocator_;
};

TEST_F(PartitionAllocMemoryReclaimerTest, FreesMemory) {
  PartitionRoot<internal::ThreadSafe>* root = allocator_->root();

  size_t committed_initially = root->get_total_size_of_committed_pages();
  AllocateAndFree();
  size_t committed_before = root->get_total_size_of_committed_pages();

  EXPECT_GT(committed_before, committed_initially);

  Reclaim();
  size_t committed_after = root->get_total_size_of_committed_pages();
  EXPECT_LT(committed_after, committed_before);
  EXPECT_LE(committed_initially, committed_after);
}

TEST_F(PartitionAllocMemoryReclaimerTest, Reclaim) {
  PartitionRoot<internal::ThreadSafe>* root = allocator_->root();
  size_t committed_initially = root->get_total_size_of_committed_pages();

  {
    AllocateAndFree();

    size_t committed_before = root->get_total_size_of_committed_pages();
    EXPECT_GT(committed_before, committed_initially);
    PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
    size_t committed_after = root->get_total_size_of_committed_pages();

    EXPECT_LT(committed_after, committed_before);
    EXPECT_LE(committed_initially, committed_after);
  }
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)

namespace {
// malloc() / free() pairs can be removed by the compiler, this is enough (for
// now) to prevent that.
NOINLINE void FreeForTest(void* data) {
  free(data);
}
}  // namespace

TEST_F(PartitionAllocMemoryReclaimerTest, DoNotAlwaysPurgeThreadCache) {
  // Make sure the thread cache is enabled in the main partition.
  base::internal::PartitionAllocMalloc::Allocator()
      ->EnableThreadCacheIfSupported();

  for (size_t i = 0; i < internal::ThreadCache::kDefaultSizeThreshold; i++) {
    void* data = malloc(i);
    FreeForTest(data);
  }

  auto* tcache = internal::ThreadCache::Get();
  ASSERT_TRUE(tcache);
  size_t cached_size = tcache->CachedMemory();

  Reclaim();

  // No thread cache purging during periodic purge, but with ReclaimAll().
  //
  // Cannot assert on the exact size of the thread cache, since it can shrink
  // when a buffer is overfull, and this may happen through other malloc()
  // allocations in the test harness.
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  Reclaim();
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
  EXPECT_LT(tcache->CachedMemory(), cached_size / 2);
}

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
        // defined(PA_THREAD_CACHE_SUPPORTED)

}  // namespace base
#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
