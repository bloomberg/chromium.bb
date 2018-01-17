// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WebTaskRunner.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace web_frame_scheduler_impl_unittest {

class WebFrameSchedulerImplTest : public ::testing::Test {
 public:
  WebFrameSchedulerImplTest() = default;
  ~WebFrameSchedulerImplTest() override = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(&clock_, true);
    scheduler_.reset(new RendererSchedulerImpl(
        CreateTaskQueueManagerForTest(nullptr, mock_task_runner_, &clock_)));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
    web_frame_scheduler_ = web_view_scheduler_->CreateWebFrameSchedulerImpl(
        nullptr, WebFrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return web_frame_scheduler_->ThrottleableTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return web_frame_scheduler_->LoadingTaskQueue();
  }

  scoped_refptr<TaskQueue> DeferrableTaskQueue() {
    return web_frame_scheduler_->DeferrableTaskQueue();
  }

  scoped_refptr<TaskQueue> PausableTaskQueue() {
    return web_frame_scheduler_->PausableTaskQueue();
  }

  scoped_refptr<TaskQueue> UnpausableTaskQueue() {
    return web_frame_scheduler_->UnpausableTaskQueue();
  }

  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

namespace {

class MockThrottlingObserver final : public WebFrameScheduler::Observer {
 public:
  MockThrottlingObserver()
      : throttled_count_(0u), not_throttled_count_(0u), stopped_count_(0u) {}

  void CheckObserverState(size_t throttled_count_expectation,
                          size_t not_throttled_count_expectation,
                          size_t stopped_count_expectation) {
    EXPECT_EQ(throttled_count_expectation, throttled_count_);
    EXPECT_EQ(not_throttled_count_expectation, not_throttled_count_);
    EXPECT_EQ(stopped_count_expectation, stopped_count_);
  }

  void OnThrottlingStateChanged(
      WebFrameScheduler::ThrottlingState state) override {
    switch (state) {
      case WebFrameScheduler::ThrottlingState::kThrottled:
        throttled_count_++;
        break;
      case WebFrameScheduler::ThrottlingState::kNotThrottled:
        not_throttled_count_++;
        break;
      case WebFrameScheduler::ThrottlingState::kStopped:
        stopped_count_++;
        break;
        // We should not have another state, and compiler checks it.
    }
  }

 private:
  size_t throttled_count_;
  size_t not_throttled_count_;
  size_t stopped_count_;
};

void RunRepeatingTask(scoped_refptr<TaskQueue> task_queue, int* run_count);

base::Closure MakeRepeatingTask(scoped_refptr<TaskQueue> task_queue,
                                int* run_count) {
  return base::Bind(&RunRepeatingTask, base::Passed(std::move(task_queue)),
                    base::Unretained(run_count));
}

void RunRepeatingTask(scoped_refptr<TaskQueue> task_queue, int* run_count) {
  ++*run_count;

  TaskQueue* task_queue_ptr = task_queue.get();
  task_queue_ptr->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(std::move(task_queue), run_count),
      TimeDelta::FromMilliseconds(1));
}

void IncrementCounter(int* counter) {
  ++*counter;
}

}  // namespace

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInForeground) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(true);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInBackground) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(true);
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_SameOrigin) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(true);
  web_frame_scheduler_->SetFrameVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameVisible_CrossOrigin) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(true);
  web_frame_scheduler_->SetFrameVisible(true);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_CrossOrigin) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(true);
  web_frame_scheduler_->SetFrameVisible(false);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, PageInBackground_ThrottlingDisabled) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(false);
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest,
       RepeatingTimer_FrameHidden_CrossOrigin_ThrottlingDisabled) {
  ScopedTimerThrottlingForHiddenFramesForTest
      timer_throttling_for_hidden_frames(false);
  web_frame_scheduler_->SetFrameVisible(false);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&IncrementCounter, base::Unretained(&counter)));

  web_frame_scheduler_->SetPaused(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, counter);

  web_frame_scheduler_->SetPaused(false);

  EXPECT_EQ(1, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);
}

// Tests if throttling observer interfaces work.
TEST_F(WebFrameSchedulerImplTest, ThrottlingObserver) {
  std::unique_ptr<MockThrottlingObserver> observer =
      std::make_unique<MockThrottlingObserver>();

  size_t throttled_count = 0u;
  size_t not_throttled_count = 0u;
  size_t stopped_count = 0u;

  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);

  web_frame_scheduler_->AddThrottlingObserver(
      WebFrameScheduler::ObserverType::kLoader, observer.get());

  // Initial state should be synchronously notified here.
  // We assume kNotThrottled is notified as an initial state, but it could
  // depend on implementation details and can be changed.
  observer->CheckObserverState(throttled_count, ++not_throttled_count,
                               stopped_count);

  // Once the page gets to be invisible, it should notify the observer of
  // kThrottled synchronously.
  web_view_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(++throttled_count, not_throttled_count,
                               stopped_count);

  // When no state has changed, observers are not called.
  web_view_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);

  // Setting background page to STOPPED, notifies observers of kStopped.
  web_view_scheduler_->SetPageStopped(true);
  observer->CheckObserverState(throttled_count, not_throttled_count,
                               ++stopped_count);

  // When page is not in the STOPPED state, then page visibility is used,
  // notifying observer of kThrottled.
  web_view_scheduler_->SetPageStopped(false);
  observer->CheckObserverState(++throttled_count, not_throttled_count,
                               stopped_count);

  // Going back to visible state should notify the observer of kNotThrottled
  // synchronously.
  web_view_scheduler_->SetPageVisible(true);
  observer->CheckObserverState(throttled_count, ++not_throttled_count,
                               stopped_count);

  // Remove from the observer list, and see if any other callback should not be
  // invoked when the condition is changed.
  web_frame_scheduler_->RemoveThrottlingObserver(
      WebFrameScheduler::ObserverType::kLoader, observer.get());
  web_view_scheduler_->SetPageVisible(false);

  // Wait 100 secs virtually and run pending tasks just in case.
  clock_.Advance(base::TimeDelta::FromSeconds(100));
  mock_task_runner_->RunUntilIdle();

  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);
}

}  // namespace web_frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
