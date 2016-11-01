// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

#include "platform/scheduler/test/fake_web_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

void increment(int* x) {
  ++*x;
}

void getIsActive(bool* isActive, RefPtr<TaskHandle>* handle) {
  *isActive = (*handle)->isActive();
}

}  // namespace

TEST(WebTaskRunnerTest, PostCancellableTaskTest) {
  scheduler::FakeWebTaskRunner taskRunner;

  // Run without cancellation.
  int count = 0;
  RefPtr<TaskHandle> handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle->isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle->isActive());

  count = 0;
  handle = taskRunner.postDelayedCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)), 1);
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle->isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle->isActive());

  // Cancel a task.
  count = 0;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  handle->cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle->isActive());
  taskRunner.runUntilIdle();
  EXPECT_EQ(0, count);

  // The task should be valid even when the handle is dropped.
  count = 0;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&increment, WTF::unretained(&count)));
  EXPECT_TRUE(handle->isActive());
  handle = nullptr;
  EXPECT_EQ(0, count);
  taskRunner.runUntilIdle();
  EXPECT_EQ(1, count);

  // handle->isActive() should switch to false before the task starts running.
  bool isActive = false;
  handle = taskRunner.postCancellableTask(
      BLINK_FROM_HERE, WTF::bind(&getIsActive, WTF::unretained(&isActive),
                                 WTF::unretained(&handle)));
  EXPECT_TRUE(handle->isActive());
  taskRunner.runUntilIdle();
  EXPECT_FALSE(isActive);
  EXPECT_FALSE(handle->isActive());
}

}  // namespace blink
