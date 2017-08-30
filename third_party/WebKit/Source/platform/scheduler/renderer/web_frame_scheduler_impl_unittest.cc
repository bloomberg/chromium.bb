// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class WebFrameSchedulerImplTest : public ::testing::Test {
 public:
  WebFrameSchedulerImplTest() {}
  ~WebFrameSchedulerImplTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::WrapUnique(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
    web_frame_scheduler_ =
        web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

namespace {

class MockThrottlingObserver : public WebFrameScheduler::Observer {
 public:
  MockThrottlingObserver() : throttled_count_(0u), not_throttled_count_(0u) {}

  void CheckObserverState(size_t throttled_count_expectation,
                          size_t not_throttled_count_expectation) {
    EXPECT_EQ(throttled_count_expectation, throttled_count_);
    EXPECT_EQ(not_throttled_count_expectation, not_throttled_count_);
  }

  // WebFrameScheduler::Observer.
  void OnThrottlingStateChanged(
      WebFrameScheduler::ThrottlingState state) override {
    switch (state) {
      case WebFrameScheduler::ThrottlingState::kThrottled:
        throttled_count_++;
        break;
      case WebFrameScheduler::ThrottlingState::kNotThrottled:
        not_throttled_count_++;
        break;
        // We should not have another state, and compiler checks it.
    }
  }

 private:
  size_t throttled_count_;
  size_t not_throttled_count_;
};

void RunRepeatingTask(RefPtr<WebTaskRunner> task_runner, int* run_count);

WTF::Closure MakeRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner,
                               int* run_count) {
  return WTF::Bind(&RunRepeatingTask, WTF::Passed(std::move(task_runner)),
                   WTF::Unretained(run_count));
}

void RunRepeatingTask(RefPtr<WebTaskRunner> task_runner, int* run_count) {
  ++*run_count;

  WebTaskRunner* task_runner_ptr = task_runner.Get();
  task_runner_ptr->PostDelayedTask(
      BLINK_FROM_HERE, MakeRepeatingTask(std::move(task_runner), run_count),
      TimeDelta::FromMilliseconds(1));
}

void IncrementCounter(int* counter) {
  ++*counter;
}

}  // namespace

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInForeground) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(true);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_PageInBackground) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(true);
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_SameOrigin) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->SetFrameVisible(false);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameVisible_CrossOrigin) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->SetFrameVisible(true);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, RepeatingTimer_FrameHidden_CrossOrigin) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(true);
  web_frame_scheduler_->SetFrameVisible(false);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest, PageInBackground_ThrottlingDisabled) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(false);
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebFrameSchedulerImplTest,
       RepeatingTimer_FrameHidden_CrossOrigin_ThrottlingDisabled) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(false);
  web_frame_scheduler_->SetFrameVisible(false);
  web_frame_scheduler_->SetCrossOrigin(true);

  int run_count = 0;
  web_frame_scheduler_->ThrottleableTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->ThrottleableTaskRunner(),
                        &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebFrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  web_frame_scheduler_->LoadingTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&IncrementCounter, WTF::Unretained(&counter)));
  web_frame_scheduler_->ThrottleableTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&IncrementCounter, WTF::Unretained(&counter)));
  web_frame_scheduler_->DeferrableTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&IncrementCounter, WTF::Unretained(&counter)));
  web_frame_scheduler_->PausableTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&IncrementCounter, WTF::Unretained(&counter)));
  web_frame_scheduler_->UnpausableTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&IncrementCounter, WTF::Unretained(&counter)));

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
      base::MakeUnique<MockThrottlingObserver>();

  size_t throttled_count = 0u;
  size_t not_throttled_count = 0u;

  observer->CheckObserverState(throttled_count, not_throttled_count);

  web_frame_scheduler_->AddThrottlingObserver(
      WebFrameScheduler::ObserverType::kLoader, observer.get());

  // Initial state should be synchronously notified here.
  // We assume kNotThrottled is notified as an initial state, but it could
  // depend on implementation details and can be changed.
  observer->CheckObserverState(throttled_count, ++not_throttled_count);

  // Once the page gets to be invisible, it should notify the observer of
  // kThrottled synchronously.
  web_view_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(++throttled_count, not_throttled_count);

  // Going back to visible state should notify the observer of kNotThrottled
  // synchronously.
  web_view_scheduler_->SetPageVisible(true);
  observer->CheckObserverState(throttled_count, ++not_throttled_count);

  // Remove from the observer list, and see if any other callback should not be
  // invoked when the condition is changed.
  web_frame_scheduler_->RemoveThrottlingObserver(
      WebFrameScheduler::ObserverType::kLoader, observer.get());
  web_view_scheduler_->SetPageVisible(false);

  // Wait 100 secs virtually and run pending tasks just in case.
  clock_->Advance(base::TimeDelta::FromSeconds(100));
  mock_task_runner_->RunUntilIdle();

  observer->CheckObserverState(throttled_count, not_throttled_count);
}

}  // namespace scheduler
}  // namespace blink
