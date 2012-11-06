// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/recursive_lock.h"

#include <stdlib.h>

#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::kNullThreadHandle;
using base::PlatformThread;
using base::PlatformThreadHandle;
using base::TimeDelta;

namespace rlz_lib {

// Basic test to make sure that Acquire()/Release() don't crash.
class BasicLockTestThread : public PlatformThread::Delegate {
 public:
  BasicLockTestThread(RecursiveLock* lock) : lock_(lock), acquired_(0) {}

  virtual void ThreadMain() OVERRIDE {
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      lock_->Release();
    }
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 20));
      lock_->Release();
    }
  }

  int acquired() const { return acquired_; }

 private:
  RecursiveLock* lock_;
  int acquired_;

  DISALLOW_COPY_AND_ASSIGN(BasicLockTestThread);
};

TEST(RecursiveLockTest, Basic) {
  RecursiveLock lock;
  BasicLockTestThread thread(&lock);
  PlatformThreadHandle handle = kNullThreadHandle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  int acquired = 0;
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 20));
    lock.Release();
  }
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 20));
    lock.Release();
  }

  PlatformThread::Join(handle);

  EXPECT_EQ(acquired, 20);
  EXPECT_EQ(thread.acquired(), 20);
}

// Tests that locks are actually exclusive.
class MutexLockTestThread : public PlatformThread::Delegate {
 public:
  MutexLockTestThread(RecursiveLock* lock, int* value)
      : lock_(lock),
        value_(value) {
  }

  // Static helper which can also be called from the main thread.
  static void DoStuff(RecursiveLock* lock, int* value) {
    for (int i = 0; i < 40; i++) {
      lock->Acquire();
      int v = *value;
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 10));
      *value = v + 1;
      lock->Release();
    }
  }

  virtual void ThreadMain() OVERRIDE {
    DoStuff(lock_, value_);
  }

 private:
  RecursiveLock* lock_;
  int* value_;

  DISALLOW_COPY_AND_ASSIGN(MutexLockTestThread);
};

TEST(RecursiveLockTest, MutexTwoThreads) {
  RecursiveLock lock;
  int value = 0;

  MutexLockTestThread thread(&lock, &value);
  PlatformThreadHandle handle = kNullThreadHandle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  MutexLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle);

  EXPECT_EQ(2 * 40, value);
}

TEST(RecursiveLockTest, MutexFourThreads) {
  RecursiveLock lock;
  int value = 0;

  MutexLockTestThread thread1(&lock, &value);
  MutexLockTestThread thread2(&lock, &value);
  MutexLockTestThread thread3(&lock, &value);
  PlatformThreadHandle handle1 = kNullThreadHandle;
  PlatformThreadHandle handle2 = kNullThreadHandle;
  PlatformThreadHandle handle3 = kNullThreadHandle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread1, &handle1));
  ASSERT_TRUE(PlatformThread::Create(0, &thread2, &handle2));
  ASSERT_TRUE(PlatformThread::Create(0, &thread3, &handle3));

  MutexLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle1);
  PlatformThread::Join(handle2);
  PlatformThread::Join(handle3);

  EXPECT_EQ(4 * 40, value);
}

// Tests that locks are recursive.
class MutexRecursiveLockTestThread : public PlatformThread::Delegate {
 public:
  MutexRecursiveLockTestThread(RecursiveLock* lock, int* value)
      : lock_(lock),
        value_(value) {
  }

  // Static helper which can also be called from the main thread.
  static void DoStuff(RecursiveLock* lock, int* value) {
    for (int i = 0; i < 20; i++) {
      // First lock.
      lock->Acquire();
      int v = *value;
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 10));
      *value = v + 1;
      {
        // Recursive lock.
        lock->Acquire();
        int v = *value;
        PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 10));
        *value = v + 1;
        lock->Release();
      }
      v = *value;
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(rand() % 10));
      *value = v + 1;
      lock->Release();
    }
  }

  virtual void ThreadMain() OVERRIDE {
    DoStuff(lock_, value_);
  }

 private:
  RecursiveLock* lock_;
  int* value_;

  DISALLOW_COPY_AND_ASSIGN(MutexRecursiveLockTestThread);
};


TEST(RecursiveLockTest, MutexTwoThreadsRecursive) {
  RecursiveLock lock;
  int value = 0;

  MutexRecursiveLockTestThread thread(&lock, &value);
  PlatformThreadHandle handle = kNullThreadHandle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  MutexRecursiveLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle);

  EXPECT_EQ(2 * 60, value);
}

TEST(RecursiveLockTest, MutexFourThreadsRecursive) {
  RecursiveLock lock;
  int value = 0;

  MutexRecursiveLockTestThread thread1(&lock, &value);
  MutexRecursiveLockTestThread thread2(&lock, &value);
  MutexRecursiveLockTestThread thread3(&lock, &value);
  PlatformThreadHandle handle1 = kNullThreadHandle;
  PlatformThreadHandle handle2 = kNullThreadHandle;
  PlatformThreadHandle handle3 = kNullThreadHandle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread1, &handle1));
  ASSERT_TRUE(PlatformThread::Create(0, &thread2, &handle2));
  ASSERT_TRUE(PlatformThread::Create(0, &thread3, &handle3));

  MutexRecursiveLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle1);
  PlatformThread::Join(handle2);
  PlatformThread::Join(handle3);

  EXPECT_EQ(4 * 60, value);
}

}  // namespace rlz_lib
