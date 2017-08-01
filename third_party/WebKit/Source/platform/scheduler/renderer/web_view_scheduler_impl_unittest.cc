// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using VirtualTimePolicy = blink::WebViewScheduler::VirtualTimePolicy;

namespace blink {
namespace scheduler {

class WebViewSchedulerImplTest : public ::testing::Test {
 public:
  WebViewSchedulerImplTest() {}
  ~WebViewSchedulerImplTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::MakeUnique<TestTimeSource>(clock_.get()));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(),
                                 DisableBackgroundTimerThrottling()));
    web_frame_scheduler_ =
        web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  virtual bool DisableBackgroundTimerThrottling() const { return false; }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersBefore) {
  std::unique_ptr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->CreateFrameScheduler(nullptr));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->CreateFrameScheduler(nullptr));
}

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  std::unique_ptr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->CreateFrameScheduler(nullptr));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->CreateFrameScheduler(nullptr));
  web_view_scheduler_.reset();
}

namespace {

void RunRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner, int* run_count);

WTF::Closure MakeRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner,
                               int* run_count) {
  return WTF::Bind(&RunRepeatingTask, WTF::Passed(std::move(task_runner)),
                   WTF::Unretained(run_count));
}

void RunRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner,
                      int* run_count) {
  ++*run_count;
  blink::WebTaskRunner* task_runner_ptr = task_runner.Get();
  task_runner_ptr->PostDelayedTask(
      BLINK_FROM_HERE, MakeRepeatingTask(std::move(task_runner), run_count),
      TimeDelta::FromMilliseconds(1));
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, RepeatingTimer_PageInForeground) {
  web_view_scheduler_->SetPageVisible(true);

  int run_count = 0;
  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->TimerTaskRunner(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest,
       RepeatingTimer_PageInBackgroundThenForeground) {
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->TimerTaskRunner(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);

  // Make sure there's no delay in throttling being removed for pages that have
  // become visible.
  web_view_scheduler_->SetPageVisible(true);

  run_count = 0;
  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1001, run_count);  // Note we end up running 1001 here because the
  // task was posted while throttled with a delay of 1ms so the first task was
  // due to run before the 1s period started.
}

TEST_F(WebViewSchedulerImplTest, RepeatingLoadingTask_PageInBackground) {
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->LoadingTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->LoadingTaskRunner(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(WebViewSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler2(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler2->CreateWebFrameSchedulerImpl(nullptr);

  web_view_scheduler_->SetPageVisible(true);
  web_view_scheduler2->SetPageVisible(false);

  int run_count1 = 0;
  int run_count2 = 0;
  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->TimerTaskRunner(), &run_count1),
      TimeDelta::FromMilliseconds(1));
  web_frame_scheduler2->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler2->TimerTaskRunner(), &run_count2),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count1);
  EXPECT_EQ(1, run_count2);
}

namespace {

void RunVirtualTimeRecorderTask(base::SimpleTestTickClock* clock,
                                RefPtr<blink::WebTaskRunner> web_task_runner,
                                std::vector<base::TimeTicks>* out_real_times,
                                std::vector<size_t>* out_virtual_times_ms) {
  out_real_times->push_back(clock->NowTicks());
  out_virtual_times_ms->push_back(
      web_task_runner->MonotonicallyIncreasingVirtualTimeSeconds() * 1000.0);
}

WTF::Closure MakeVirtualTimeRecorderTask(
    base::SimpleTestTickClock* clock,
    RefPtr<blink::WebTaskRunner> web_task_runner,
    std::vector<base::TimeTicks>* out_real_times,
    std::vector<size_t>* out_virtual_times_ms) {
  return WTF::Bind(&RunVirtualTimeRecorderTask, WTF::Unretained(clock),
                   WTF::Passed(std::move(web_task_runner)),
                   WTF::Unretained(out_real_times),
                   WTF::Unretained(out_virtual_times_ms));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_TimerFastForwarding) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      web_frame_scheduler_->TimerTaskRunner()
          ->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(20));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times_ms, ElementsAre(initial_virtual_time_ms + 2,
                                            initial_virtual_time_ms + 20,
                                            initial_virtual_time_ms + 200));
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_LoadingTaskFastForwarding) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      web_frame_scheduler_->TimerTaskRunner()
          ->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler_->LoadingTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->LoadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  web_frame_scheduler_->LoadingTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->LoadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(20));

  web_frame_scheduler_->LoadingTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->LoadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times_ms, ElementsAre(initial_virtual_time_ms + 2,
                                            initial_virtual_time_ms + 20,
                                            initial_virtual_time_ms + 200));
}

TEST_F(WebViewSchedulerImplTest,
       RepeatingTimer_PageInBackground_MeansNothingForVirtualTime) {
  web_view_scheduler_->EnableVirtualTime();
  web_view_scheduler_->SetPageVisible(false);
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();

  int run_count = 0;
  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->TimerTaskRunner(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunTasksWhile(mock_task_runner_->TaskRunCountBelow(2000));
  // Virtual time means page visibility is ignored.
  EXPECT_EQ(1999, run_count);

  // The global tick clock has not moved, yet we ran a large number of "delayed"
  // tasks despite calling setPageVisible(false).
  EXPECT_EQ(initial_real_time, scheduler_->tick_clock()->NowTicks());
}

namespace {

void RunOrderTask(int index, std::vector<int>* out_run_order) {
  out_run_order->push_back(index);
}

void DelayedRunOrderTask(int index,
                         RefPtr<blink::WebTaskRunner> task_runner,
                         std::vector<int>* out_run_order) {
  out_run_order->push_back(index);
  task_runner->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&RunOrderTask, index + 1, WTF::Unretained(out_run_order)));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_NotAllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::PAUSE);
  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler_->TimerTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&RunOrderTask, 0, WTF::Unretained(&run_order)));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      WTF::Bind(&DelayedRunOrderTask, 1,
                WTF::Passed(web_frame_scheduler_->TimerTaskRunner()),
                WTF::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(2));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      WTF::Bind(&DelayedRunOrderTask, 3,
                WTF::Passed(web_frame_scheduler_->TimerTaskRunner()),
                WTF::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(4));

  mock_task_runner_->RunUntilIdle();

  // No timer tasks are allowed to run.
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_AllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::ADVANCE);
  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler_->TimerTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&RunOrderTask, 0, WTF::Unretained(&run_order)));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      WTF::Bind(&DelayedRunOrderTask, 1,
                WTF::Passed(web_frame_scheduler_->TimerTaskRunner()),
                WTF::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(2));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      WTF::Bind(&DelayedRunOrderTask, 3,
                WTF::Passed(web_frame_scheduler_->TimerTaskRunner()),
                WTF::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(4));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 1, 2, 3, 4));
}

class WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling
    : public WebViewSchedulerImplTest {
 public:
  WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() {}
  ~WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() override {}

  bool DisableBackgroundTimerThrottling() const override { return true; }
};

TEST_F(WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling,
       RepeatingTimer_PageInBackground) {
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeRepeatingTask(web_frame_scheduler_->TimerTaskRunner(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimeSettings_NewWebFrameScheduler) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::PAUSE);
  web_view_scheduler_->EnableVirtualTime();

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);

  web_frame_scheduler->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE, WTF::Bind(&RunOrderTask, 1, WTF::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::ADVANCE);
  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1));
}

namespace {

template <typename T>
WTF::Closure MakeDeletionTask(T* obj) {
  return WTF::Bind([](T* obj) { delete obj; }, WTF::Unretained(obj));
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, DeleteWebFrameSchedulers_InTask) {
  for (int i = 0; i < 10; i++) {
    WebFrameSchedulerImpl* web_frame_scheduler =
        web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr).release();
    web_frame_scheduler->TimerTaskRunner()->PostDelayedTask(
        BLINK_FROM_HERE, MakeDeletionTask(web_frame_scheduler),
        TimeDelta::FromMilliseconds(1));
  }
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteWebViewScheduler_InTask) {
  web_frame_scheduler_->TimerTaskRunner()->PostTask(
      BLINK_FROM_HERE, MakeDeletionTask(web_view_scheduler_.release()));
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteThrottledQueue_InTask) {
  web_view_scheduler_->SetPageVisible(false);

  WebFrameSchedulerImpl* web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr).release();
  RefPtr<blink::WebTaskRunner> timer_task_runner =
      web_frame_scheduler->TimerTaskRunner();

  int run_count = 0;
  timer_task_runner->PostDelayedTask(
      BLINK_FROM_HERE, MakeRepeatingTask(timer_task_runner, &run_count),
      TimeDelta::FromMilliseconds(1));

  // Note this will run at time t = 10s since we start at time t = 5000us, and
  // it will prevent further tasks from running (i.e. the RepeatingTask) by
  // deleting the WebFrameScheduler.
  timer_task_runner->PostDelayedTask(BLINK_FROM_HERE,
                                     MakeDeletionTask(web_frame_scheduler),
                                     TimeDelta::FromMilliseconds(9990));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(100));
  EXPECT_EQ(10, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimePolicy_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  // Initially virtual time is not allowed to advance until we have seen at
  // least one load.
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(3u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(3u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(4u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(4u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, RedundantDidStopLoadingCallsAreHarmless) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(2u);
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, BackgroundParser_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  // Initially virtual time is not allowed to advance until we have seen at
  // least one load.
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, ProvisionalLoads) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  web_view_scheduler_->DidStartLoading(1u);
  web_view_scheduler_->DidStopLoading(1u);

  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  std::unique_ptr<WebFrameSchedulerImpl> frame1 =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);
  std::unique_ptr<WebFrameSchedulerImpl> frame2 =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);

  // Provisional loads are refcounted.
  web_view_scheduler_->DidBeginProvisionalLoad(frame1.get());
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidBeginProvisionalLoad(frame2.get());
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidEndProvisionalLoad(frame2.get());
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidEndProvisionalLoad(frame1.get());
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, WillNavigateBackForwardSoon) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  web_view_scheduler_->DidStartLoading(1u);
  web_view_scheduler_->DidStopLoading(1u);

  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  std::unique_ptr<WebFrameSchedulerImpl> frame =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);

  web_view_scheduler_->WillNavigateBackForwardSoon(frame.get());
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidBeginProvisionalLoad(frame.get());
  EXPECT_FALSE(web_view_scheduler_->VirtualTimeAllowedToAdvance());

  web_view_scheduler_->DidEndProvisionalLoad(frame.get());
  EXPECT_TRUE(web_view_scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, PauseTimersWhileVirtualTimeIsPaused) {
  std::vector<int> run_order;

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);
  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::PAUSE);
  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler->TimerTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&RunOrderTask, 1, WTF::Unretained(&run_order)));

  mock_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::ADVANCE);
  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(WebViewSchedulerImplTest, VirtualTimeBudgetExhaustedCallback) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      web_frame_scheduler_->TimerTaskRunner()
          ->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(1));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(5));

  web_frame_scheduler_->TimerTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      MakeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->TimerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(7));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(5),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::PAUSE);
          },
          WTF::Unretained(web_view_scheduler_.get())));

  mock_task_runner_->RunUntilIdle();

  // The timer that is scheduled for the exact point in time when virtual time
  // expires will not run.
  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times_ms, ElementsAre(initial_virtual_time_ms + 1,
                                            initial_virtual_time_ms + 2,
                                            initial_virtual_time_ms + 5));
}

namespace {

using ScopedExpensiveBackgroundTimerThrottlingForTest =
    ScopedRuntimeEnabledFeatureForTest<
        RuntimeEnabledFeatures::ExpensiveBackgroundTimerThrottlingEnabled,
        RuntimeEnabledFeatures::SetExpensiveBackgroundTimerThrottlingEnabled>;

void ExpensiveTestTask(base::SimpleTestTickClock* clock,
                       std::vector<base::TimeTicks>* run_times) {
  run_times->push_back(clock->NowTicks());
  clock->Advance(base::TimeDelta::FromMilliseconds(250));
}

void InitializeTrialParams() {
  std::map<std::string, std::string> params = {{"cpu_budget", "0.01"},
                                               {"max_budget", "0.0"},
                                               {"initial_budget", "0.0"},
                                               {"max_delay", "0.0"}};
  const char kParamName[] = "ExpensiveBackgroundTimerThrottling";
  const char kGroupName[] = "Enabled";
  EXPECT_TRUE(base::AssociateFieldTrialParams(kParamName, kGroupName, params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(kParamName, kGroupName));

  std::map<std::string, std::string> actual_params;
  base::GetFieldTrialParams(kParamName, &actual_params);
  EXPECT_EQ(actual_params, params);
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, BackgroundTimerThrottling) {
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  std::unique_ptr<base::FieldTrialList> field_trial_list =
      base::MakeUnique<base::FieldTrialList>(nullptr);
  InitializeTrialParams();
  web_view_scheduler_.reset(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));

  std::vector<base::TimeTicks> run_times;
  web_frame_scheduler_ =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(nullptr);
  web_view_scheduler_->SetPageVisible(true);

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(2500));

  web_frame_scheduler_->TimerTaskRunner()
      ->ToSingleThreadTaskRunner()
      ->PostDelayedTask(
          BLINK_FROM_HERE,
          base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
          TimeDelta::FromMilliseconds(1));
  web_frame_scheduler_->TimerTaskRunner()
      ->ToSingleThreadTaskRunner()
      ->PostDelayedTask(
          BLINK_FROM_HERE,
          base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
          TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(3500));

  // Check that these tasks are aligned, but are not subject to budget-based
  // throttling.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(2501),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2751)));
  run_times.clear();

  web_view_scheduler_->SetPageVisible(false);

  web_frame_scheduler_->TimerTaskRunner()
      ->ToSingleThreadTaskRunner()
      ->PostDelayedTask(
          BLINK_FROM_HERE,
          base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
          TimeDelta::FromMicroseconds(1));
  web_frame_scheduler_->TimerTaskRunner()
      ->ToSingleThreadTaskRunner()
      ->PostDelayedTask(
          BLINK_FROM_HERE,
          base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
          TimeDelta::FromMicroseconds(1));

  mock_task_runner_->RunUntilIdle();

  // Check that tasks are aligned and throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(4),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(26)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

TEST_F(WebViewSchedulerImplTest, OpenWebSocketExemptsFromBudgetThrottling) {
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  std::unique_ptr<base::FieldTrialList> field_trial_list =
      base::MakeUnique<base::FieldTrialList>(nullptr);
  InitializeTrialParams();
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));

  std::vector<base::TimeTicks> run_times;

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler1 =
      web_view_scheduler->CreateWebFrameSchedulerImpl(nullptr);
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler->CreateWebFrameSchedulerImpl(nullptr);

  web_view_scheduler->SetPageVisible(false);

  // Wait for 20s to avoid initial throttling delay.
  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(20500));

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler1->TimerTaskRunner()
        ->ToSingleThreadTaskRunner()
        ->PostDelayedTask(
            BLINK_FROM_HERE,
            base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
            TimeDelta::FromMilliseconds(1));
  }

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(55500));

  // Check that tasks are throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(21),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(26),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(51)));
  run_times.clear();

  std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
      websocket_connection = web_frame_scheduler1->OnActiveConnectionCreated();

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler1->TimerTaskRunner()
        ->ToSingleThreadTaskRunner()
        ->PostDelayedTask(
            BLINK_FROM_HERE,
            base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
            TimeDelta::FromMilliseconds(1));
  }

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(58500));

  // Check that the timer task queue from the first frame is aligned,
  // but not throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56250),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56500)));
  run_times.clear();

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler2->TimerTaskRunner()
        ->ToSingleThreadTaskRunner()
        ->PostDelayedTask(
            BLINK_FROM_HERE,
            base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
            TimeDelta::FromMilliseconds(1));
  }

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(59500));

  // Check that the second frame scheduler becomes unthrottled.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59250),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59500)));
  run_times.clear();

  websocket_connection.reset();

  // Wait for 10s to enable throttling back.
  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(70500));

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler1->TimerTaskRunner()
        ->ToSingleThreadTaskRunner()
        ->PostDelayedTask(
            BLINK_FROM_HERE,
            base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
            TimeDelta::FromMilliseconds(1));
  }

  mock_task_runner_->RunUntilIdle();

  // WebSocket is closed, budget-based throttling now applies.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(84),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(109),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(134)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

}  // namespace scheduler
}  // namespace blink
