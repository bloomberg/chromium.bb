// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WebTaskRunner.h"

#include "platform/scheduler/test/fake_web_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

void increment(int* x) {
  ++*x;
}

void getIsActive(bool* isActive, TaskHandle* handle) {
  *isActive = handle->isActive();
}

class CancellationTestHelper {
 public:
  CancellationTestHelper() : m_weakPtrFactory(this) {}

  WeakPtr<CancellationTestHelper> createWeakPtr() {
    return m_weakPtrFactory.createWeakPtr();
  }

  void revokeWeakPtrs() { m_weakPtrFactory.revokeAll(); }
  void incrementCounter() { ++m_counter; }
  int counter() const { return m_counter; }

 private:
  int m_counter = 0;
  WeakPtrFactory<CancellationTestHelper> m_weakPtrFactory;
};

}  // namespace

TEST(WebTaskRunnerTest, PostCancellableTaskTest) {
  scheduler::FakeWebTaskRunner taskRunner;

  // Run without cancellation.
  int count = 0;
  TaskHandle handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.isActive());

  count = 0;
  handle = taskRunner.postDelayedCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)), 1);
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.isActive());

  // Cancel a task.
  count = 0;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  handle.cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle.isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(0, count);

  // The task should be cancelled when the handle is dropped.
  {
    count = 0;
    TaskHandle handle2 = taskRunner.postCancellableTask(
        BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
    EXPECT_TRUE(handle2.isActive());
  }
  EXPECT_EQ(0, count);
  taskRunner.runUntilIdle();
  EXPECT_EQ(0, count);

  // The task should be cancelled when another TaskHandle is assigned on it.
  count = 0;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  handle = taskRunner.postCancellableTask(BLINK_FROM_HERE, WTF::bind([] {}));
  EXPECT_EQ(0, count);
  taskRunner.runUntilIdle();
  EXPECT_EQ(0, count);

  // Self assign should be nop.
  count = 0;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
#if COMPILER(CLANG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  handle = std::move(handle);
#pragma GCC diagnostic pop
#else
  handle = std::move(handle);
#endif  // COMPILER(CLANG)
  EXPECT_EQ(0, count);
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);

  // handle->isActive() should switch to false before the task starts running.
  bool isActive = false;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&getIsActive, WTF::unretained(&isActive),
                                 WTF::unretained(&handle)));
  EXPECT_TRUE(handle.isActive());
  taskRunner.runUntilIdle();
  EXPECT_FALSE(isActive);
  EXPECT_FALSE(handle.isActive());
}

TEST(WebTaskRunnerTest, CancellationCheckerTest) {
  scheduler::FakeWebTaskRunner taskRunner;

  int count = 0;
  TaskHandle handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  EXPECT_EQ(0, count);

  // TaskHandle::isActive should detect the deletion of posted task.
  auto queue = taskRunner.takePendingTasksForTesting();
  ASSERT_EQ(1u, queue.size());
  EXPECT_FALSE(queue[0].IsCancelled());
  EXPECT_TRUE(handle.isActive());
  queue.clear();
  EXPECT_FALSE(handle.isActive());
  EXPECT_EQ(0, count);

  count = 0;
  CancellationTestHelper helper;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&CancellationTestHelper::incrementCounter,
                                 helper.createWeakPtr()));
  EXPECT_EQ(0, helper.counter());

  // The cancellation of the posted task should be propagated to TaskHandle.
  queue = taskRunner.takePendingTasksForTesting();
  ASSERT_EQ(1u, queue.size());
  EXPECT_FALSE(queue[0].IsCancelled());
  EXPECT_TRUE(handle.isActive());
  helper.revokeWeakPtrs();
  EXPECT_TRUE(queue[0].IsCancelled());
  EXPECT_FALSE(handle.isActive());
}

}  // namespace blink
