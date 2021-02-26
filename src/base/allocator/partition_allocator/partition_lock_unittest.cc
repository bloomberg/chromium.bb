// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_lock.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(RepeatingClosure f) : f_(f) {}
  void ThreadMain() override { f_.Run(); }

 private:
  RepeatingClosure f_;
};

TEST(SpinLockTest, Simple) {
  MaybeSpinLock<true> lock;
  lock.Lock();
  lock.Unlock();
}

MaybeSpinLock<true> g_lock;
TEST(SpinLockTest, StaticLockStartsUnlocked) {
  g_lock.Lock();
  g_lock.Unlock();
}

TEST(SpinLockTest, Contended) {
  int counter = 0;  // *Not* atomic.
  std::vector<PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 1000000;
  constexpr int num_threads = 4;

  MaybeSpinLock<true> lock;
  MaybeSpinLock<true> start_lock;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    start_lock.Lock();
    start_lock.Unlock();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Lock();
      counter++;
      lock.Unlock();
    }
  })};

  start_lock.Lock();  // Make sure that the threads compete, by waiting until
                      // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    PlatformThreadHandle handle;
    PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Unlock();

  for (int i = 0; i < num_threads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(SpinLockTest, SlowThreads) {
  int counter = 0;  // *Not* atomic.
  std::vector<PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 100;
  constexpr int num_threads = 4;

  MaybeSpinLock<true> lock;
  MaybeSpinLock<true> start_lock;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    start_lock.Lock();
    start_lock.Unlock();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Lock();
      counter++;
      // Hold the lock for a while, to force futex()-based locks to sleep.
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
      lock.Unlock();
    }
  })};

  start_lock.Lock();  // Make sure that the threads compete, by waiting until
                      // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    PlatformThreadHandle handle;
    PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Unlock();

  for (int i = 0; i < num_threads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

}  // namespace
}  // namespace internal
}  // namespace base
