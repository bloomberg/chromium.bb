// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace frame_scheduler_impl_unittest {

class FrameSchedulerImplTest : public testing::Test {
 public:
  FrameSchedulerImplTest() = default;
  ~FrameSchedulerImplTest() override = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(&clock_, true);
    scheduler_.reset(new MainThreadSchedulerImpl(
        TaskQueueManagerForTest::Create(nullptr, mock_task_runner_, &clock_),
        base::nullopt));
    page_scheduler_.reset(
        new PageSchedulerImpl(nullptr, scheduler_.get(), false));
    frame_scheduler_ = page_scheduler_->CreateFrameSchedulerImpl(
        nullptr, FrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  scoped_refptr<TaskQueue> throttleable_task_queue() {
    return frame_scheduler_->throttleable_task_queue_;
  }

  void LazyInitThrottleableTaskQueue() {
    EXPECT_FALSE(throttleable_task_queue());
    frame_scheduler_->ThrottleableTaskQueue();
    EXPECT_TRUE(throttleable_task_queue());
  }

  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return frame_scheduler_->ThrottleableTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return frame_scheduler_->LoadingTaskQueue();
  }

  scoped_refptr<TaskQueue> DeferrableTaskQueue() {
    return frame_scheduler_->DeferrableTaskQueue();
  }

  scoped_refptr<TaskQueue> PausableTaskQueue() {
    return frame_scheduler_->PausableTaskQueue();
  }

  scoped_refptr<TaskQueue> UnpausableTaskQueue() {
    return frame_scheduler_->UnpausableTaskQueue();
  }

  bool IsThrottled() {
    EXPECT_TRUE(throttleable_task_queue());
    return scheduler_->task_queue_throttler()->IsThrottled(
        throttleable_task_queue().get());
  }

  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
};

namespace {

class MockThrottlingObserver final : public FrameScheduler::Observer {
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
      FrameScheduler::ThrottlingState state) override {
    switch (state) {
      case FrameScheduler::ThrottlingState::kThrottled:
        throttled_count_++;
        break;
      case FrameScheduler::ThrottlingState::kNotThrottled:
        not_throttled_count_++;
        break;
      case FrameScheduler::ThrottlingState::kStopped:
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

void IncrementCounter(int* counter) {
  ++*counter;
}

}  // namespace

// Throttleable task queue is initialized lazily, so there're two scenarios:
// - Task queue created first and throttling decision made later;
// - Scheduler receives relevant signals to make a throttling decision but
//   applies one once task queue gets created.
// We test both (ExplicitInit/LazyInit) of them.

TEST_F(FrameSchedulerImplTest, PageVisible) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  EXPECT_FALSE(throttleable_task_queue());
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  page_scheduler_->SetPageVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHiddenThenVisible_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHiddenThenVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  frame_scheduler_->SetCrossOrigin(false);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_TRUE(IsThrottled());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHidden_CrossOrigin_NoThrottling_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_NoThrottling_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  EXPECT_TRUE(throttleable_task_queue());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(true);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPaused(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, counter);

  frame_scheduler_->SetPaused(false);

  EXPECT_EQ(1, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndUnfreezeFlagEnabled) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(true);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPageVisibility(PageVisibilityState::kHidden);
  frame_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  // unpausable tasks continue to run.
  EXPECT_EQ(1, counter);

  frame_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(1, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndUnfreezeFlagDisabled) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(false);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(false);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPageVisibility(PageVisibilityState::kHidden);
  frame_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  // throttleable tasks are frozen, other tasks continue to run.
  EXPECT_EQ(4, counter);

  frame_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(4, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeWithKeepActive) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(false);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetKeepActive(true);  // say we have a Service Worker
  frame_scheduler_->SetPageVisibility(PageVisibilityState::kHidden);
  frame_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  // Everything runs except throttleable tasks (timers)
  EXPECT_EQ(4, counter);

  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  EXPECT_EQ(4, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);  // loading task runs

  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  // KeepActive is false when Service Worker stops.
  frame_scheduler_->SetKeepActive(false);
  EXPECT_EQ(5, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);  // loading task does not run

  frame_scheduler_->SetKeepActive(true);
  EXPECT_EQ(5, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(6, counter);  // loading task runs
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndPageVisible) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(true);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPageVisibility(PageVisibilityState::kHidden);
  frame_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, counter);

  // Making the page visible should cause frozen queues to resume.
  frame_scheduler_->SetPageVisibility(PageVisibilityState::kVisible);

  EXPECT_EQ(1, counter);
  mock_task_runner_->RunUntilIdle();
  EXPECT_EQ(5, counter);
}

// Tests if throttling observer interfaces work.
TEST_F(FrameSchedulerImplTest, ThrottlingObserver) {
  std::unique_ptr<MockThrottlingObserver> observer =
      std::make_unique<MockThrottlingObserver>();

  size_t throttled_count = 0u;
  size_t not_throttled_count = 0u;
  size_t stopped_count = 0u;

  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);

  auto observer_handle = frame_scheduler_->AddThrottlingObserver(
      FrameScheduler::ObserverType::kLoader, observer.get());

  // Initial state should be synchronously notified here.
  // We assume kNotThrottled is notified as an initial state, but it could
  // depend on implementation details and can be changed.
  observer->CheckObserverState(throttled_count, ++not_throttled_count,
                               stopped_count);

  // Once the page gets to be invisible, it should notify the observer of
  // kThrottled synchronously.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(++throttled_count, not_throttled_count,
                               stopped_count);

  // When no state has changed, observers are not called.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);

  // Setting background page to STOPPED, notifies observers of kStopped.
  page_scheduler_->SetPageFrozen(true);
  observer->CheckObserverState(throttled_count, not_throttled_count,
                               ++stopped_count);

  // When page is not in the STOPPED state, then page visibility is used,
  // notifying observer of kThrottled.
  page_scheduler_->SetPageFrozen(false);
  observer->CheckObserverState(++throttled_count, not_throttled_count,
                               stopped_count);

  // Going back to visible state should notify the observer of kNotThrottled
  // synchronously.
  page_scheduler_->SetPageVisible(true);
  observer->CheckObserverState(throttled_count, ++not_throttled_count,
                               stopped_count);

  // Remove from the observer list, and see if any other callback should not be
  // invoked when the condition is changed.
  observer_handle.reset();
  page_scheduler_->SetPageVisible(false);

  // Wait 100 secs virtually and run pending tasks just in case.
  clock_.Advance(base::TimeDelta::FromSeconds(100));
  mock_task_runner_->RunUntilIdle();

  observer->CheckObserverState(throttled_count, not_throttled_count,
                               stopped_count);
}

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
