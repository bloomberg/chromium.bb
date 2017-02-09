// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using VirtualTimePolicy = blink::WebViewScheduler::VirtualTimePolicy;

namespace blink {
namespace scheduler {

class WebViewSchedulerImplTest : public testing::Test {
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
        web_view_scheduler_->createWebFrameSchedulerImpl(nullptr);
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
      web_view_scheduler_->createFrameScheduler(nullptr));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->createFrameScheduler(nullptr));
}

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  std::unique_ptr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->createFrameScheduler(nullptr));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->createFrameScheduler(nullptr));
  web_view_scheduler_.reset();
}

namespace {

void runRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner, int* run_count);

std::unique_ptr<WTF::Closure> makeRepeatingTask(
    RefPtr<blink::WebTaskRunner> task_runner,
    int* run_count) {
  return WTF::bind(&runRepeatingTask, WTF::passed(std::move(task_runner)),
                   WTF::unretained(run_count));
}

void runRepeatingTask(RefPtr<blink::WebTaskRunner> task_runner,
                      int* run_count) {
  ++*run_count;
  blink::WebTaskRunner* task_runner_ptr = task_runner.get();
  task_runner_ptr->postDelayedTask(
      BLINK_FROM_HERE, makeRepeatingTask(std::move(task_runner), run_count),
      1.0);
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, RepeatingTimer_PageInForeground) {
  web_view_scheduler_->setPageVisible(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest,
       RepeatingTimer_PageInBackgroundThenForeground) {
  web_view_scheduler_->setPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);

  // The task queue isn't throttled at all until it's been in the background for
  // a 10 second grace period.
  clock_->Advance(base::TimeDelta::FromSeconds(10));

  run_count = 0;
  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);

  // Make sure there's no delay in throttling being removed for pages that have
  // become visible.
  web_view_scheduler_->setPageVisible(true);

  run_count = 0;
  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1001, run_count);  // Note we end up running 1001 here because the
  // task was posted while throttled with a delay of 1ms so the first task was
  // due to run before the 1s period started.
}

TEST_F(WebViewSchedulerImplTest, GracePeriodAppliesToNewBackgroundFrames) {
  web_view_scheduler_->setPageVisible(false);

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->createWebFrameSchedulerImpl(nullptr);
  blink::WebTaskRunner* timer_task_runner =
      web_frame_scheduler->timerTaskRunner().get();

  int run_count = 0;
  timer_task_runner->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);

  // The task queue isn't throttled at all until it's been in the background for
  // a 10 second grace period.
  clock_->Advance(base::TimeDelta::FromSeconds(10));

  run_count = 0;
  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);

  // Make sure there's no delay in throttling being removed for pages that have
  // become visible.
  web_view_scheduler_->setPageVisible(true);

  run_count = 0;
  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1001, run_count);  // Note we end up running 1001 here because the
  // task was posted while throttled with a delay of 1ms so the first task was
  // due to run before the 1s period started.
}

TEST_F(WebViewSchedulerImplTest, RepeatingLoadingTask_PageInBackground) {
  web_view_scheduler_->setPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->loadingTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->loadingTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(WebViewSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler2(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler2->createWebFrameSchedulerImpl(nullptr);

  web_view_scheduler_->setPageVisible(true);
  web_view_scheduler2->setPageVisible(false);

  // Advance past the no-throttling grace period.
  clock_->Advance(base::TimeDelta::FromSeconds(10));

  int run_count1 = 0;
  int run_count2 = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count1),
      1.0);
  web_frame_scheduler2->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler2->timerTaskRunner(), &run_count2),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count1);
  EXPECT_EQ(1, run_count2);
}

namespace {

void runVirtualTimeRecorderTask(base::SimpleTestTickClock* clock,
                                RefPtr<blink::WebTaskRunner> web_task_runner,
                                std::vector<base::TimeTicks>* out_real_times,
                                std::vector<size_t>* out_virtual_times_ms) {
  out_real_times->push_back(clock->NowTicks());
  out_virtual_times_ms->push_back(
      web_task_runner->monotonicallyIncreasingVirtualTimeSeconds() * 1000.0);
}

std::unique_ptr<WTF::Closure> makeVirtualTimeRecorderTask(
    base::SimpleTestTickClock* clock,
    RefPtr<blink::WebTaskRunner> web_task_runner,
    std::vector<base::TimeTicks>* out_real_times,
    std::vector<size_t>* out_virtual_times_ms) {
  return WTF::bind(&runVirtualTimeRecorderTask, WTF::unretained(clock),
                   WTF::passed(std::move(web_task_runner)),
                   WTF::unretained(out_real_times),
                   WTF::unretained(out_virtual_times_ms));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_TimerFastForwarding) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      web_frame_scheduler_->timerTaskRunner()
          ->monotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->enableVirtualTime();

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->timerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      2.0);

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->timerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      20.0);

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->timerTaskRunner(),
                                  &real_times, &virtual_times_ms),
      200.0);

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
      web_frame_scheduler_->timerTaskRunner()
          ->monotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->enableVirtualTime();

  web_frame_scheduler_->loadingTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->loadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      2.0);

  web_frame_scheduler_->loadingTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->loadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      20.0);

  web_frame_scheduler_->loadingTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeVirtualTimeRecorderTask(clock_.get(),
                                  web_frame_scheduler_->loadingTaskRunner(),
                                  &real_times, &virtual_times_ms),
      200.0);

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times_ms, ElementsAre(initial_virtual_time_ms + 2,
                                            initial_virtual_time_ms + 20,
                                            initial_virtual_time_ms + 200));
}

TEST_F(WebViewSchedulerImplTest,
       RepeatingTimer_PageInBackground_MeansNothingForVirtualTime) {
  web_view_scheduler_->enableVirtualTime();
  web_view_scheduler_->setPageVisible(false);
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunTasksWhile(mock_task_runner_->TaskRunCountBelow(2000));
  // Virtual time means page visibility is ignored.
  EXPECT_EQ(1999, run_count);

  // The global tick clock has not moved, yet we ran a large number of "delayed"
  // tasks despite calling setPageVisible(false).
  EXPECT_EQ(initial_real_time, scheduler_->tick_clock()->NowTicks());
}

namespace {

void runOrderTask(int index, std::vector<int>* out_run_order) {
  out_run_order->push_back(index);
}

void delayedRunOrderTask(int index,
                         RefPtr<blink::WebTaskRunner> task_runner,
                         std::vector<int>* out_run_order) {
  out_run_order->push_back(index);
  task_runner->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&runOrderTask, index + 1, WTF::unretained(out_run_order)));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_NotAllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->setVirtualTimePolicy(VirtualTimePolicy::PAUSE);
  web_view_scheduler_->enableVirtualTime();

  web_frame_scheduler_->timerTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&runOrderTask, 0, WTF::unretained(&run_order)));

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      WTF::bind(&delayedRunOrderTask, 1,
                WTF::passed(web_frame_scheduler_->timerTaskRunner()),
                WTF::unretained(&run_order)),
      2.0);

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      WTF::bind(&delayedRunOrderTask, 3,
                WTF::passed(web_frame_scheduler_->timerTaskRunner()),
                WTF::unretained(&run_order)),
      4.0);

  mock_task_runner_->RunUntilIdle();

  // Immediate tasks are allowed to run even if delayed tasks are not.
  EXPECT_THAT(run_order, ElementsAre(0));
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_AllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->setVirtualTimePolicy(VirtualTimePolicy::ADVANCE);
  web_view_scheduler_->enableVirtualTime();

  web_frame_scheduler_->timerTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&runOrderTask, 0, WTF::unretained(&run_order)));

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      WTF::bind(&delayedRunOrderTask, 1,
                WTF::passed(web_frame_scheduler_->timerTaskRunner()),
                WTF::unretained(&run_order)),
      2.0);

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      WTF::bind(&delayedRunOrderTask, 3,
                WTF::passed(web_frame_scheduler_->timerTaskRunner()),
                WTF::unretained(&run_order)),
      4.0);

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
  web_view_scheduler_->setPageVisible(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      makeRepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimeSettings_NewWebFrameScheduler) {
  std::vector<int> run_order;

  web_view_scheduler_->setVirtualTimePolicy(VirtualTimePolicy::PAUSE);
  web_view_scheduler_->enableVirtualTime();

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->createWebFrameSchedulerImpl(nullptr);

  web_frame_scheduler->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, WTF::bind(&runOrderTask, 1, WTF::unretained(&run_order)),
      1);

  mock_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  web_view_scheduler_->setVirtualTimePolicy(VirtualTimePolicy::ADVANCE);
  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1));
}

namespace {

template <typename T>
std::unique_ptr<WTF::Closure> makeDeletionTask(T* obj) {
  return WTF::bind([](T* obj) { delete obj; }, WTF::unretained(obj));
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, DeleteWebFrameSchedulers_InTask) {
  for (int i = 0; i < 10; i++) {
    WebFrameSchedulerImpl* web_frame_scheduler =
        web_view_scheduler_->createWebFrameSchedulerImpl(nullptr).release();
    web_frame_scheduler->timerTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE, makeDeletionTask(web_frame_scheduler), 1);
  }
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteWebViewScheduler_InTask) {
  web_frame_scheduler_->timerTaskRunner()->postTask(
      BLINK_FROM_HERE, makeDeletionTask(web_view_scheduler_.release()));
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteThrottledQueue_InTask) {
  web_view_scheduler_->setPageVisible(false);

  // The task queue isn't throttled at all until it's been in the background for
  // a 10 second grace period.
  clock_->Advance(base::TimeDelta::FromSeconds(10));

  WebFrameSchedulerImpl* web_frame_scheduler =
      web_view_scheduler_->createWebFrameSchedulerImpl(nullptr).release();
  RefPtr<blink::WebTaskRunner> timer_task_runner =
      web_frame_scheduler->timerTaskRunner();

  int run_count = 0;
  timer_task_runner->postDelayedTask(
      BLINK_FROM_HERE, makeRepeatingTask(timer_task_runner, &run_count), 1.0);

  // Note this will run at time t = 10s since we start at time t = 5000us, and
  // it will prevent further tasks from running (i.e. the RepeatingTask) by
  // deleting the WebFrameScheduler.
  timer_task_runner->postDelayedTask(
      BLINK_FROM_HERE, makeDeletionTask(web_frame_scheduler), 9990.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(100));
  EXPECT_EQ(10, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimePolicy_DETERMINISTIC_LOADING) {
  web_view_scheduler_->setVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  // Initially virtual time is not allowed to advance until we have seen at
  // least one load.
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(3u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(3u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(4u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(4u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, RedundantDidStopLoadingCallsAreHarmless) {
  web_view_scheduler_->setVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(2u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(2u);
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, BackgroundParser_DETERMINISTIC_LOADING) {
  web_view_scheduler_->setVirtualTimePolicy(
      VirtualTimePolicy::DETERMINISTIC_LOADING);
  // Initially virtual time is not allowed to advance until we have seen at
  // least one load.
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStartLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DidStopLoading(1u);
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->IncrementBackgroundParserCount();
  EXPECT_FALSE(web_view_scheduler_->virtualTimeAllowedToAdvance());

  web_view_scheduler_->DecrementBackgroundParserCount();
  EXPECT_TRUE(web_view_scheduler_->virtualTimeAllowedToAdvance());
}

namespace {

using ScopedExpensiveBackgroundTimerThrottlingForTest =
    ScopedRuntimeEnabledFeatureForTest<
        RuntimeEnabledFeatures::expensiveBackgroundTimerThrottlingEnabled,
        RuntimeEnabledFeatures::setExpensiveBackgroundTimerThrottlingEnabled>;

void ExpensiveTestTask(base::SimpleTestTickClock* clock,
                       std::vector<base::TimeTicks>* run_times) {
  run_times->push_back(clock->NowTicks());
  clock->Advance(base::TimeDelta::FromMilliseconds(250));
}

class FakeWebViewSchedulerSettings
    : public WebViewScheduler::WebViewSchedulerSettings {
 public:
  float expensiveBackgroundThrottlingCPUBudget() override { return 0.01; }

  float expensiveBackgroundThrottlingInitialBudget() override { return 0.0; }

  float expensiveBackgroundThrottlingMaxBudget() override { return 0.0; }

  float expensiveBackgroundThrottlingMaxDelay() override { return 0.0; }
};

}  // namespace

TEST_F(WebViewSchedulerImplTest, BackgroundThrottlingGracePeriod) {
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  std::vector<base::TimeTicks> run_times;
  FakeWebViewSchedulerSettings web_view_scheduler_settings;
  web_view_scheduler_.reset(new WebViewSchedulerImpl(
      nullptr, &web_view_scheduler_settings, scheduler_.get(), false));
  web_frame_scheduler_ =
      web_view_scheduler_->createWebFrameSchedulerImpl(nullptr);
  web_view_scheduler_->setPageVisible(false);

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(2500));

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
      1);
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
      1);

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(3500));

  // Check that these tasks are initially unthrottled.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(2501),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2751)));
  run_times.clear();

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(11500));

  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
      1);
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, base::Bind(&ExpensiveTestTask, clock_.get(), &run_times),
      1);

  mock_task_runner_->RunUntilIdle();

  // After the grace period has passed, tasks should be aligned and have budget
  // based throttling.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(12),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(26)));
}

TEST_F(WebViewSchedulerImplTest, OpenWebSocketExemptsFromBudgetThrottling) {
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  std::vector<base::TimeTicks> run_times;
  FakeWebViewSchedulerSettings web_view_scheduler_settings;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler(
      new WebViewSchedulerImpl(nullptr, &web_view_scheduler_settings,
                               scheduler_.get(), false));

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler1 =
      web_view_scheduler->createWebFrameSchedulerImpl(nullptr);
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler->createWebFrameSchedulerImpl(nullptr);

  web_view_scheduler->setPageVisible(false);

  // Wait for 20s to avoid initial throttling delay.
  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(20500));

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler1->timerTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE,
        base::Bind(&ExpensiveTestTask, clock_.get(), &run_times), 1);
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
      websocket_connection = web_frame_scheduler1->onActiveConnectionCreated();

  for (size_t i = 0; i < 3; ++i) {
    web_frame_scheduler1->timerTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE,
        base::Bind(&ExpensiveTestTask, clock_.get(), &run_times), 1);
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
    web_frame_scheduler2->timerTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE,
        base::Bind(&ExpensiveTestTask, clock_.get(), &run_times), 1);
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
    web_frame_scheduler1->timerTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE,
        base::Bind(&ExpensiveTestTask, clock_.get(), &run_times), 1);
  }

  mock_task_runner_->RunUntilIdle();

  // WebSocket is closed, budget-based throttling now applies.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(84),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(109),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(134)));
}

}  // namespace scheduler
}  // namespace blink
