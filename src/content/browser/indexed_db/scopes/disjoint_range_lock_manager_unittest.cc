// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <content/browser/indexed_db/scopes/disjoint_range_lock_manager.h>
#include <content/test/barrier_builder.h>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
using ScopeLock = ScopesLockManager::ScopeLock;
using LockRange = ScopesLockManager::LockRange;

template <typename T>
void SetValue(T* out, T value) {
  *out = value;
}

std::string IntegerKey(size_t num) {
  return base::StringPrintf("%010zd", num);
}

void StoreLock(ScopeLock* lock_out, base::OnceClosure done, ScopeLock lock) {
  (*lock_out) = std::move(lock);
  std::move(done).Run();
}

class DisjointRangeLockManagerTest : public testing::Test {
 public:
  DisjointRangeLockManagerTest() = default;
  ~DisjointRangeLockManagerTest() override = default;

 private:
  base::test::ScopedTaskEnvironment task_env_;
};

TEST_F(DisjointRangeLockManagerTest, BasicAcquisition) {
  const size_t kTotalLocks = 1000;
  DisjointRangeLockManager lock_manager(1, leveldb::BytewiseComparator(),
                                        base::SequencedTaskRunnerHandle::Get());

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  base::RunLoop loop;
  std::vector<ScopeLock> locks;
  locks.resize(kTotalLocks);
  {
    BarrierBuilder barrier(loop.QuitClosure());

    for (size_t i = 0; i < kTotalLocks / 2; ++i) {
      LockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      lock_manager.AcquireLock(
          0, range, ScopesLockManager::LockType::kExclusive,
          base::BindOnce(&StoreLock, &locks[i], barrier.AddClosure()));
    }

    for (size_t i = kTotalLocks - 1; i >= kTotalLocks / 2; --i) {
      LockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      lock_manager.AcquireLock(
          0, range, ScopesLockManager::LockType::kExclusive,
          base::BindOnce(&StoreLock, &locks[i], barrier.AddClosure()));
    }
  }
  loop.Run();
  EXPECT_EQ(static_cast<int64_t>(kTotalLocks),
            lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  // All locks should be acquired.
  for (const auto& lock : locks) {
    EXPECT_TRUE(lock.is_locked());
  }

  locks.clear();
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
}

TEST_F(DisjointRangeLockManagerTest, Shared) {
  DisjointRangeLockManager lock_manager(1, leveldb::BytewiseComparator(),
                                        base::SequencedTaskRunnerHandle::Get());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  LockRange range = {IntegerKey(0), IntegerKey(1)};

  ScopeLock lock1;
  ScopeLock lock2;
  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    lock_manager.AcquireLock(
        0, range, ScopesLockManager::LockType::kShared,
        base::BindOnce(&StoreLock, &lock1, barrier.AddClosure()));
    lock_manager.AcquireLock(
        0, range, ScopesLockManager::LockType::kShared,
        base::BindOnce(&StoreLock, &lock2, barrier.AddClosure()));
  }
  loop.Run();
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());

  EXPECT_TRUE(lock1.is_locked());
  EXPECT_TRUE(lock2.is_locked());
}

TEST_F(DisjointRangeLockManagerTest, SharedAndExclusiveQueuing) {
  DisjointRangeLockManager lock_manager(1, leveldb::BytewiseComparator(),
                                        base::SequencedTaskRunnerHandle::Get());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  LockRange range = {IntegerKey(0), IntegerKey(1)};

  ScopeLock shared_lock1;
  ScopeLock shared_lock2;
  ScopeLock exclusive_lock3;
  ScopeLock shared_lock4;

  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    lock_manager.AcquireLock(
        0, range, ScopesLockManager::LockType::kShared,
        base::BindOnce(&StoreLock, &shared_lock1, barrier));
    lock_manager.AcquireLock(
        0, range, ScopesLockManager::LockType::kShared,
        base::BindOnce(&StoreLock, &shared_lock2, barrier));
    loop.Run();
  }

  // Both of the following locks should be queued - the exclusive is next in
  // line, then the shared lock will come after it.
  lock_manager.AcquireLock(
      0, range, ScopesLockManager::LockType::kExclusive,
      base::BindOnce(&StoreLock, &exclusive_lock3, base::DoNothing::Once()));
  lock_manager.AcquireLock(
      0, range, ScopesLockManager::LockType::kShared,
      base::BindOnce(&StoreLock, &shared_lock4, base::DoNothing::Once()));
  // Flush the task queue.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(exclusive_lock3.is_locked());
  EXPECT_FALSE(shared_lock4.is_locked());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(2ll, lock_manager.RequestsWaitingForTesting());

  // Release the shared locks.
  shared_lock1.Release();
  shared_lock2.Release();
  EXPECT_FALSE(shared_lock1.is_locked());
  EXPECT_FALSE(shared_lock2.is_locked());

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(exclusive_lock3.is_locked());
  EXPECT_FALSE(shared_lock4.is_locked());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(1ll, lock_manager.RequestsWaitingForTesting());

  exclusive_lock3.Release();
  EXPECT_FALSE(exclusive_lock3.is_locked());

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(shared_lock4.is_locked());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

TEST_F(DisjointRangeLockManagerTest, LevelsOperateSeparately) {
  DisjointRangeLockManager lock_manager(2, leveldb::BytewiseComparator(),
                                        base::SequencedTaskRunnerHandle::Get());
  base::RunLoop loop;
  ScopeLock l0_lock;
  ScopeLock l1_lock;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    LockRange range = {IntegerKey(0), IntegerKey(1)};
    lock_manager.AcquireLock(
        0, range, ScopesLockManager::LockType::kExclusive,
        base::BindOnce(&StoreLock, &l0_lock, barrier.AddClosure()));
    lock_manager.AcquireLock(
        1, range, ScopesLockManager::LockType::kExclusive,
        base::BindOnce(&StoreLock, &l1_lock, barrier.AddClosure()));
  }
  loop.Run();
  EXPECT_TRUE(l0_lock.is_locked());
  EXPECT_TRUE(l1_lock.is_locked());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  l0_lock.Release();
  l1_lock.Release();
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
}

}  // namespace
}  // namespace content
