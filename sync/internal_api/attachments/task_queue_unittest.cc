// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/task_queue.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace syncer {

namespace {

const TimeDelta kZero;

}  // namespace

class TaskQueueTest : public testing::Test {
 protected:
  TaskQueueTest() : weak_ptr_factory_(this) {
    queue_.reset(new TaskQueue<int>(
        base::Bind(&TaskQueueTest::Process, weak_ptr_factory_.GetWeakPtr()),
        TimeDelta::FromMinutes(1),
        TimeDelta::FromMinutes(8)));
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void Process(const int& task) { dispatched_.push_back(task); }

  base::MessageLoop message_loop_;
  scoped_ptr<TaskQueue<int> > queue_;
  std::vector<int> dispatched_;
  base::WeakPtrFactory<TaskQueueTest> weak_ptr_factory_;
};

// See that at most one task is dispatched at a time.
TEST_F(TaskQueueTest, AddToQueue_NoConcurrentTasks) {
  queue_->AddToQueue(1);
  queue_->AddToQueue(2);
  RunLoop();

  // Only one has been dispatched.
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  RunLoop();

  // Still only one.
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(1);
  RunLoop();

  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(2, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(2);
  RunLoop();

  ASSERT_TRUE(dispatched_.empty());
}

// See that that the queue ignores duplicate adds.
TEST_F(TaskQueueTest, AddToQueue_NoDuplicates) {
  queue_->AddToQueue(1);
  queue_->AddToQueue(1);
  queue_->AddToQueue(2);
  queue_->AddToQueue(1);
  ASSERT_TRUE(dispatched_.empty());
  RunLoop();

  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(1);
  RunLoop();

  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(2, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(2);
  RunLoop();

  ASSERT_TRUE(dispatched_.empty());
}

// See that Retry works as expected.
TEST_F(TaskQueueTest, Retry) {
  scoped_ptr<base::MockTimer> timer_to_pass(new base::MockTimer(false, false));
  base::MockTimer* mock_timer = timer_to_pass.get();
  queue_->SetTimerForTest(timer_to_pass.PassAs<base::Timer>());

  // 1st attempt.
  queue_->AddToQueue(1);
  ASSERT_TRUE(mock_timer->IsRunning());
  ASSERT_EQ(kZero, mock_timer->GetCurrentDelay());
  TimeDelta last_delay = mock_timer->GetCurrentDelay();
  mock_timer->Fire();
  RunLoop();

  // 2nd attempt.
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsFailed(1);
  queue_->AddToQueue(1);
  ASSERT_TRUE(mock_timer->IsRunning());
  EXPECT_GT(mock_timer->GetCurrentDelay(), last_delay);
  EXPECT_LE(mock_timer->GetCurrentDelay(), TimeDelta::FromMinutes(1));
  last_delay = mock_timer->GetCurrentDelay();
  mock_timer->Fire();
  RunLoop();

  // 3rd attempt.
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsFailed(1);
  queue_->AddToQueue(1);
  ASSERT_TRUE(mock_timer->IsRunning());
  EXPECT_GT(mock_timer->GetCurrentDelay(), last_delay);
  last_delay = mock_timer->GetCurrentDelay();
  mock_timer->Fire();
  RunLoop();

  // Give up.
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->Cancel(1);
  ASSERT_FALSE(mock_timer->IsRunning());

  // Try a different task.  See the timer remains unchanged because the previous
  // task was cancelled.
  ASSERT_TRUE(dispatched_.empty());
  queue_->AddToQueue(2);
  ASSERT_TRUE(mock_timer->IsRunning());
  EXPECT_GE(last_delay, mock_timer->GetCurrentDelay());
  last_delay = mock_timer->GetCurrentDelay();
  mock_timer->Fire();
  RunLoop();

  // Mark this one as succeeding, which will clear the backoff delay.
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(2, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(2);
  ASSERT_FALSE(mock_timer->IsRunning());

  // Add one last task and see that it's dispatched without delay because the
  // previous one succeeded.
  ASSERT_TRUE(dispatched_.empty());
  queue_->AddToQueue(3);
  ASSERT_TRUE(mock_timer->IsRunning());
  EXPECT_LT(mock_timer->GetCurrentDelay(), last_delay);
  last_delay = mock_timer->GetCurrentDelay();
  mock_timer->Fire();
  RunLoop();

  // Clean up.
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(3, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(3);
  ASSERT_FALSE(mock_timer->IsRunning());
}

TEST_F(TaskQueueTest, Cancel) {
  queue_->AddToQueue(1);
  RunLoop();

  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->Cancel(1);
  RunLoop();

  ASSERT_TRUE(dispatched_.empty());
}

// See that ResetBackoff resets the backoff delay.
TEST_F(TaskQueueTest, ResetBackoff) {
  scoped_ptr<base::MockTimer> timer_to_pass(new base::MockTimer(false, false));
  base::MockTimer* mock_timer = timer_to_pass.get();
  queue_->SetTimerForTest(timer_to_pass.PassAs<base::Timer>());

  // Add an item, mark it as failed, re-add it and see that we now have a
  // backoff delay.
  queue_->AddToQueue(1);
  ASSERT_TRUE(mock_timer->IsRunning());
  ASSERT_EQ(kZero, mock_timer->GetCurrentDelay());
  mock_timer->Fire();
  RunLoop();
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsFailed(1);
  queue_->AddToQueue(1);
  ASSERT_TRUE(mock_timer->IsRunning());
  EXPECT_GT(mock_timer->GetCurrentDelay(), kZero);
  EXPECT_LE(mock_timer->GetCurrentDelay(), TimeDelta::FromMinutes(1));

  // Call ResetBackoff and see that there is no longer a delay.
  queue_->ResetBackoff();
  ASSERT_TRUE(mock_timer->IsRunning());
  ASSERT_EQ(kZero, mock_timer->GetCurrentDelay());
  mock_timer->Fire();
  RunLoop();
  ASSERT_FALSE(mock_timer->IsRunning());
  ASSERT_EQ(1U, dispatched_.size());
  EXPECT_EQ(1, dispatched_.front());
  dispatched_.clear();
  queue_->MarkAsSucceeded(1);
}

}  // namespace syncer
