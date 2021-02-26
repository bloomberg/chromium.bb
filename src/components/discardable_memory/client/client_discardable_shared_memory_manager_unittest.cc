// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

using base::Location;
using base::OnceClosure;
using base::TimeDelta;

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
  ~TestSingleThreadTaskRunner() override = default;
  bool PostTask(const Location& from_here, OnceClosure task) { return true; }
  template <class T>
  bool DeleteSoon(const Location& from_here, const T* object) {
    return true;
  }
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    return true;
  }
  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override {
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }
};

class TestClientDiscardableSharedMemoryManager
    : public ClientDiscardableSharedMemoryManager {
 public:
  explicit TestClientDiscardableSharedMemoryManager(
      scoped_refptr<base::SingleThreadTaskRunner> periodic_purge_task_runner)
      : ClientDiscardableSharedMemoryManager(
            base::MakeRefCounted<TestSingleThreadTaskRunner>(),
            periodic_purge_task_runner) {}

  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id) override {
    auto shared_memory = std::make_unique<base::DiscardableSharedMemory>();
    shared_memory->CreateAndMap(size);
    return shared_memory;
  }

  void DeletedDiscardableSharedMemory(int32_t id) override {}

  size_t GetSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetSize();
  }

  size_t GetSizeOfFreeLists() const {
    base::AutoLock lock(lock_);
    return heap_->GetSizeOfFreeLists();
  }

  bool TimerIsNull() const {
    base::AutoLock lock(lock_);
    return timer_ == nullptr;
  }

 private:
  ~TestClientDiscardableSharedMemoryManager() override = default;
};

class ClientDiscardableSharedMemoryManagerTest : public testing::Test {
 public:
  ClientDiscardableSharedMemoryManagerTest()
      : task_env_(base::test::TaskEnvironment::MainThreadType::UI,
                  base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    client_ = base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>(
        task_env_.GetMainThreadTaskRunner());
  }
  base::test::TaskEnvironment task_env_;
  scoped_refptr<TestClientDiscardableSharedMemoryManager> client_;
  const size_t page_size_ = base::GetPageSize();
};

// This test allocates a single piece of memory, then verifies that calling
// |BackgroundPurge| only affects the memory when it is unlocked.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Simple) {
  // Initially, we should have no memory allocated
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem = client_->AllocateLockedDiscardableMemory(page_size_);

  // After allocation, we should have allocated a single piece of memory.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_);

  client_->BackgroundPurge();

  // All our memory is locked, so calling |BackgroundPurge| should have no
  // effect.
  EXPECT_EQ(client_->GetBytesAllocated(), base::GetPageSize());

  mem->Unlock();

  // Unlocking has no effect on the amount of memory we have allocated.
  EXPECT_EQ(client_->GetBytesAllocated(), base::GetPageSize());

  client_->BackgroundPurge();

  // Now that |mem| is unlocked, the call to |BackgroundPurge| will
  // remove it.
  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
}

// This test allocates multiple pieces of memory, then unlocks them one by one,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleOneByOne) {
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 2.2);
  auto mem2 = client_->AllocateLockedDiscardableMemory(page_size_ * 1.1);
  auto mem3 = client_->AllocateLockedDiscardableMemory(page_size_ * 3.5);
  auto mem4 = client_->AllocateLockedDiscardableMemory(page_size_ * 0.2);

  EXPECT_EQ(client_->GetBytesAllocated(), 10 * page_size_);

  // Does nothing because everything is locked.
  client_->BackgroundPurge();

  EXPECT_EQ(client_->GetBytesAllocated(), 10 * page_size_);

  mem1->Unlock();

  // Does nothing, since we don't have any free memory, just unlocked memory.
  client_->ReleaseFreeMemory();

  EXPECT_EQ(client_->GetBytesAllocated(), 10 * page_size_);

  // This gets rid of |mem1| (which is unlocked), but not the rest of the
  // memory.
  client_->BackgroundPurge();

  EXPECT_EQ(client_->GetBytesAllocated(), 7 * page_size_);

  // We do similar checks to above for the rest of the memory.
  mem2->Unlock();

  client_->BackgroundPurge();

  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  mem3->Unlock();

  client_->BackgroundPurge();
  EXPECT_EQ(client_->GetBytesAllocated(), 1 * page_size_);

  mem4->Unlock();

  client_->BackgroundPurge();
  EXPECT_EQ(client_->GetBytesAllocated(), 0 * page_size_);
}

// This test allocates multiple pieces of memory, then unlocks them all,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleAtOnce) {
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 2.2);
  auto mem2 = client_->AllocateLockedDiscardableMemory(page_size_ * 1.1);
  auto mem3 = client_->AllocateLockedDiscardableMemory(page_size_ * 3.5);
  auto mem4 = client_->AllocateLockedDiscardableMemory(page_size_ * 0.2);

  EXPECT_EQ(client_->GetBytesAllocated(), 10 * page_size_);

  // Does nothing because everything is locked.
  client_->BackgroundPurge();

  EXPECT_EQ(client_->GetBytesAllocated(), 10 * page_size_);

  // Unlock all pieces of memory at once.
  mem1->Unlock();
  mem2->Unlock();
  mem3->Unlock();
  mem4->Unlock();

  client_->BackgroundPurge();
  EXPECT_EQ(client_->GetBytesAllocated(), 0 * page_size_);
}

// Tests that FreeLists are only released once all memory has been released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Release) {
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client_->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client_->GetSizeOfFreeLists();
  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 3);

  client_->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 3);

  mem2 = nullptr;

  // No memory is allocated, but freelists are grown.
  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 5);

  client_->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), 0u);
}

// Similar to previous test, but makes sure that freelist still shrinks when
// last piece of memory was just unlocked instead of released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ReleaseUnlocked) {
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client_->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client_->GetSizeOfFreeLists();
  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 3);

  client_->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 3);

  mem2->Unlock();

  // No change in memory usage, since memory was only unlocked not released.
  EXPECT_EQ(client_->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), freelist_size + page_size_ * 3);

  client_->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), 0u);
}

// This tests that memory is actually removed by the periodic purging. We mock a
// task runner for this test and fast forward to make sure that the memory is
// purged at the right time.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ScheduledReleaseUnlocked) {
  base::test::ScopedFeatureList fl;
  fl.InitAndEnableFeature(discardable_memory::kSchedulePeriodicPurge);
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 3);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 3 * page_size_);

  mem1->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
}

// Same as the above test, but tests that multiple pieces of memory will be
// handled properly.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       ScheduledReleaseUnlockedMultiple) {
  base::test::ScopedFeatureList fl;
  fl.InitAndEnableFeature(discardable_memory::kSchedulePeriodicPurge);
  ASSERT_EQ(client_->GetBytesAllocated(), 0u);
  ASSERT_EQ(client_->GetSizeOfFreeLists(), 0u);

  auto mem1 = client_->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client_->AllocateLockedDiscardableMemory(page_size_ * 2);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  mem1->Unlock();

  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  // The purge only removes things that have been unlocked for at least
  // |kMinAgeForScheduledPurge|
  // minutes so this shouldn't remove anything.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 5 * page_size_);

  // The periodic purge should remove anything that's been locked for over
  // |kMinAgeForScheduledPurge|
  // minutes, so fast forward slightly more so that it happens.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 2 * page_size_);

  mem2->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client_->GetBytesAllocated(), 0u);
  EXPECT_EQ(client_->GetSizeOfFreeLists(), 0u);
}

// Tests that the UMA for Lock()-ing successes
// ("Memory.Discardable.LockingSuccess") is recorded properly.
TEST_F(ClientDiscardableSharedMemoryManagerTest, LockingSuccessUma) {
  base::HistogramTester histograms;

  // number is arbitrary, we are only focused on whether the histogram is
  // properly recorded in this test.
  auto mem = client_->AllocateLockedDiscardableMemory(200);

  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 0);

  // Unlock then lock. This should add one sample to the success bucket.
  mem->Unlock();
  bool result = mem->Lock();

  ASSERT_EQ(result, true);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 1);

  // Repeat the above to verify a second sample is added.
  mem->Unlock();
  result = mem->Lock();

  ASSERT_EQ(result, true);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 2);

  // This should now fail because the unlocked memory was purged. This should
  // add a sample to the failure bucket.
  mem->Unlock();
  client_->BackgroundPurge();
  result = mem->Lock();

  ASSERT_EQ(result, false);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 1);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 2);
}

// Test that a repeating timer for background purging is created when we
// allocate memory and discarded when we run out of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, SchedulingProactivePurging) {
  base::test::ScopedFeatureList fl;
  fl.InitAndEnableFeature(discardable_memory::kSchedulePeriodicPurge);
  ASSERT_TRUE(client_->TimerIsNull());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client_->AllocateLockedDiscardableMemory(200);

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  // This does not destroy the timer because there is still memory allocated.
  client_->ReleaseFreeMemory();

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  mem = nullptr;

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  // Now that all memory is freed, destroy the timer.
  client_->ReleaseFreeMemory();

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_TRUE(client_->TimerIsNull());
}

// This test is similar to the one above, but tests that creating and deleting
// the timer still works with multiple pieces of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       SchedulingProactivePurgingMultipleAllocations) {
  base::test::ScopedFeatureList fl;
  fl.InitAndEnableFeature(discardable_memory::kSchedulePeriodicPurge);
  ASSERT_TRUE(client_->TimerIsNull());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client_->AllocateLockedDiscardableMemory(200);
  auto mem2 = client_->AllocateLockedDiscardableMemory(100);

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  client_->ReleaseFreeMemory();

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  mem = nullptr;

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  client_->ReleaseFreeMemory();

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  mem2 = nullptr;

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_FALSE(client_->TimerIsNull());

  client_->ReleaseFreeMemory();

  task_env_.FastForwardBy(TimeDelta::FromSeconds(0));
  EXPECT_TRUE(client_->TimerIsNull());
}

}  // namespace
}  // namespace discardable_memory
