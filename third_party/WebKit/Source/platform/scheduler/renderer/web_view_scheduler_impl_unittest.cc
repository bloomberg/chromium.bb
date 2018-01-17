// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using VirtualTimePolicy = blink::WebViewScheduler::VirtualTimePolicy;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace web_view_scheduler_impl_unittest {

class WebViewSchedulerImplTest : public ::testing::Test {
 public:
  WebViewSchedulerImplTest() = default;
  ~WebViewSchedulerImplTest() override = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(&clock_, true);
    scheduler_.reset(new RendererSchedulerImpl(
        CreateTaskQueueManagerForTest(nullptr, mock_task_runner_, &clock_)));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(),
                                 DisableBackgroundTimerThrottling()));
    web_frame_scheduler_ = web_view_scheduler_->CreateWebFrameSchedulerImpl(
        nullptr, WebFrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  virtual bool DisableBackgroundTimerThrottling() const { return false; }

 protected:
  static scoped_refptr<TaskQueue> ThrottleableTaskQueueForScheduler(
      WebFrameSchedulerImpl* scheduler) {
    return scheduler->ThrottleableTaskQueue();
  }

  scoped_refptr<WebTaskRunner> ThrottleableTaskRunner() {
    return WebTaskRunnerImpl::Create(ThrottleableTaskQueue(),
                                     TaskType::kInternalTest);
  }

  scoped_refptr<WebTaskRunner> LoadingTaskRunner() {
    return WebTaskRunnerImpl::Create(LoadingTaskQueue(),
                                     TaskType::kInternalTest);
  }

  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return web_frame_scheduler_->ThrottleableTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return web_frame_scheduler_->LoadingTaskQueue();
  }

  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersBefore) {
  std::unique_ptr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->CreateFrameScheduler(
          nullptr, WebFrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->CreateFrameScheduler(
          nullptr, WebFrameScheduler::FrameType::kSubframe));
}

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  std::unique_ptr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->CreateFrameScheduler(
          nullptr, WebFrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->CreateFrameScheduler(
          nullptr, WebFrameScheduler::FrameType::kSubframe));
  web_view_scheduler_.reset();
}

namespace {

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
      FROM_HERE, MakeRepeatingTask(std::move(task_queue_ptr), run_count),
      TimeDelta::FromMilliseconds(1));
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, RepeatingTimer_PageInForeground) {
  web_view_scheduler_->SetPageVisible(true);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest,
       RepeatingTimer_PageInBackgroundThenForeground) {
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
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
  LoadingTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(LoadingTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(WebViewSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler2(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler2->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);

  web_view_scheduler_->SetPageVisible(true);
  web_view_scheduler2->SetPageVisible(false);

  int run_count1 = 0;
  int run_count2 = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count1),
      TimeDelta::FromMilliseconds(1));
  ThrottleableTaskQueueForScheduler(web_frame_scheduler2.get())
      ->PostDelayedTask(FROM_HERE,
                        MakeRepeatingTask(ThrottleableTaskQueueForScheduler(
                                              web_frame_scheduler2.get()),
                                          &run_count2),
                        TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count1);
  EXPECT_EQ(1, run_count2);
}

namespace {

void RunVirtualTimeRecorderTask(base::SimpleTestTickClock* clock,
                                scoped_refptr<WebTaskRunner> task_runner,
                                std::vector<base::TimeTicks>* out_real_times,
                                std::vector<size_t>* out_virtual_times_ms) {
  out_real_times->push_back(clock->NowTicks());
  out_virtual_times_ms->push_back(
      task_runner->MonotonicallyIncreasingVirtualTimeSeconds() * 1000.0);
}

base::OnceClosure MakeVirtualTimeRecorderTask(
    base::SimpleTestTickClock* clock,
    scoped_refptr<WebTaskRunner> task_runner,
    std::vector<base::TimeTicks>* out_real_times,
    std::vector<size_t>* out_virtual_times_ms) {
  return WTF::Bind(&RunVirtualTimeRecorderTask, WTF::Unretained(clock),
                   WTF::Passed(std::move(task_runner)),
                   WTF::Unretained(out_real_times),
                   WTF::Unretained(out_virtual_times_ms));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_TimerFastForwarding) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      ThrottleableTaskRunner()->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(20));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
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
      ThrottleableTaskRunner()->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, LoadingTaskRunner(), &real_times,
                                  &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, LoadingTaskRunner(), &real_times,
                                  &virtual_times_ms),
      TimeDelta::FromMilliseconds(20));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, LoadingTaskRunner(), &real_times,
                                  &virtual_times_ms),
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
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
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
                         scoped_refptr<TaskQueue> task_queue,
                         std::vector<int>* out_run_order) {
  out_run_order->push_back(index);
  task_queue->PostTask(FROM_HERE, base::Bind(&RunOrderTask, index + 1,
                                             base::Unretained(out_run_order)));
}
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_NotAllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedRunOrderTask, 1, base::Passed(ThrottleableTaskQueue()),
                 base::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(2));

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedRunOrderTask, 3, base::Passed(ThrottleableTaskQueue()),
                 base::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(4));

  mock_task_runner_->RunUntilIdle();

  // No timer tasks are allowed to run.
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_F(WebViewSchedulerImplTest, VirtualTime_AllowedToAdvance) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::Bind(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedRunOrderTask, 1, base::Passed(ThrottleableTaskQueue()),
                 base::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(2));

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedRunOrderTask, 3, base::Passed(ThrottleableTaskQueue()),
                 base::Unretained(&run_order)),
      TimeDelta::FromMilliseconds(4));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 1, 2, 3, 4));
}

class WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling
    : public WebViewSchedulerImplTest {
 public:
  WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() = default;
  ~WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() override =
      default;

  bool DisableBackgroundTimerThrottling() const override { return true; }
};

TEST_F(WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling,
       RepeatingTimer_PageInBackground) {
  web_view_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(ThrottleableTaskQueue(), &run_count),
      TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimeSettings_NewWebFrameScheduler) {
  std::vector<int> run_order;

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  web_view_scheduler_->EnableVirtualTime();

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);

  ThrottleableTaskQueueForScheduler(web_frame_scheduler.get())
      ->PostDelayedTask(
          FROM_HERE, base::Bind(&RunOrderTask, 1, base::Unretained(&run_order)),
          TimeDelta::FromMilliseconds(1));

  mock_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1));
}

namespace {

template <typename T>
base::Closure MakeDeletionTask(T* obj) {
  return base::Bind([](T* obj) { delete obj; }, base::Unretained(obj));
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, DeleteWebFrameSchedulers_InTask) {
  for (int i = 0; i < 10; i++) {
    WebFrameSchedulerImpl* web_frame_scheduler =
        web_view_scheduler_
            ->CreateWebFrameSchedulerImpl(
                nullptr, WebFrameScheduler::FrameType::kSubframe)
            .release();
    ThrottleableTaskQueueForScheduler(web_frame_scheduler)
        ->PostDelayedTask(FROM_HERE, MakeDeletionTask(web_frame_scheduler),
                          TimeDelta::FromMilliseconds(1));
  }
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteWebViewScheduler_InTask) {
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, MakeDeletionTask(web_view_scheduler_.release()));
  mock_task_runner_->RunUntilIdle();
}

TEST_F(WebViewSchedulerImplTest, DeleteThrottledQueue_InTask) {
  web_view_scheduler_->SetPageVisible(false);

  WebFrameSchedulerImpl* web_frame_scheduler =
      web_view_scheduler_
          ->CreateWebFrameSchedulerImpl(nullptr,
                                        WebFrameScheduler::FrameType::kSubframe)
          .release();
  scoped_refptr<TaskQueue> timer_task_queue =
      ThrottleableTaskQueueForScheduler(web_frame_scheduler);

  int run_count = 0;
  timer_task_queue->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(timer_task_queue, &run_count),
      TimeDelta::FromMilliseconds(1));

  // Note this will run at time t = 10s since we start at time t = 5000us.
  // However, we still should run all tasks after frame scheduler deletion.
  timer_task_queue->PostDelayedTask(FROM_HERE,
                                    MakeDeletionTask(web_frame_scheduler),
                                    TimeDelta::FromMilliseconds(9990));

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(100));
  EXPECT_EQ(90015, run_count);
}

TEST_F(WebViewSchedulerImplTest, VirtualTimePauseCount_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest,
       WebScopedVirtualTimePauser_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);

  {
    WebScopedVirtualTimePauser virtual_time_pauser =
        web_frame_scheduler->CreateWebScopedVirtualTimePauser();
    EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.PauseVirtualTime(true);
    EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.PauseVirtualTime(false);
    EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.PauseVirtualTime(true);
    EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());
  }

  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest,
       MultipleWebScopedVirtualTimePausers_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);

  WebScopedVirtualTimePauser virtual_time_pauser1 =
      web_frame_scheduler->CreateWebScopedVirtualTimePauser();
  WebScopedVirtualTimePauser virtual_time_pauser2 =
      web_frame_scheduler->CreateWebScopedVirtualTimePauser();

  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser1.PauseVirtualTime(true);
  virtual_time_pauser2.PauseVirtualTime(true);
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser2.PauseVirtualTime(false);
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser1.PauseVirtualTime(false);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, NestedMessageLoop_DETERMINISTIC_LOADING) {
  web_view_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->OnBeginNestedRunLoop();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->OnExitNestedRunLoop();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(WebViewSchedulerImplTest, PauseTimersWhileVirtualTimeIsPaused) {
  std::vector<int> run_order;

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler =
      web_view_scheduler_->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);
  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueueForScheduler(web_frame_scheduler.get())
      ->PostTask(FROM_HERE,
                 base::Bind(&RunOrderTask, 1, base::Unretained(&run_order)));

  mock_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(WebViewSchedulerImplTest, VirtualTimeBudgetExhaustedCallback) {
  std::vector<base::TimeTicks> real_times;
  std::vector<size_t> virtual_times_ms;
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  size_t initial_virtual_time_ms =
      ThrottleableTaskRunner()->MonotonicallyIncreasingVirtualTimeSeconds() *
      1000.0;

  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(1));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(5));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(&clock_, ThrottleableTaskRunner(),
                                  &real_times, &virtual_times_ms),
      TimeDelta::FromMilliseconds(7));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(5),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
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
class MockObserver : public WebViewScheduler::VirtualTimeObserver {
 public:
  ~MockObserver() override = default;

  void OnVirtualTimeAdvanced(base::TimeDelta virtual_time_offset) override {
    virtual_time_log_.push_back(base::StringPrintf(
        "Advanced to %dms",
        static_cast<int>(virtual_time_offset.InMilliseconds())));
  }

  void OnVirtualTimePaused(base::TimeDelta virtual_time_offset) override {
    virtual_time_log_.push_back(base::StringPrintf(
        "Paused at %dms",
        static_cast<int>(virtual_time_offset.InMilliseconds())));
  }

  const std::vector<std::string>& virtual_time_log() const {
    return virtual_time_log_;
  }

 private:
  std::vector<std::string> virtual_time_log_;
};

void NopTask() {}
}  // namespace

TEST_F(WebViewSchedulerImplTest, VirtualTimeObserver) {
  MockObserver mock_observer;
  web_view_scheduler_->AddVirtualTimeObserver(&mock_observer);
  web_view_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueue()->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                                           TimeDelta::FromMilliseconds(200));

  ThrottleableTaskQueue()->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                                           TimeDelta::FromMilliseconds(20));

  ThrottleableTaskQueue()->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                                           TimeDelta::FromMilliseconds(2));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          WTF::Unretained(web_view_scheduler_.get())));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      mock_observer.virtual_time_log(),
      ElementsAre("Advanced to 2ms", "Advanced to 20ms", "Advanced to 200ms",
                  "Advanced to 1000ms", "Paused at 1000ms"));
  web_view_scheduler_->RemoveVirtualTimeObserver(&mock_observer);
}

namespace {
void RepostingTask(scoped_refptr<TaskQueue> task_queue,
                   int max_count,
                   int* count) {
  if (++(*count) >= max_count)
    return;

  task_queue->PostTask(FROM_HERE,
                       base::Bind(&RepostingTask, task_queue, max_count,
                                  base::Unretained(count)));
}

void DelayedTask(int* count_in, int* count_out) {
  *count_out = *count_in;
}

}  // namespace

TEST_F(WebViewSchedulerImplTest, MaxVirtualTimeTaskStarvationCountOneHundred) {
  web_view_scheduler_->EnableVirtualTime();
  web_view_scheduler_->SetMaxVirtualTimeTaskStarvationCount(100);
  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue(), 1000, &count);
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(DelayedTask, base::Unretained(&count),
                 base::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          WTF::Unretained(web_view_scheduler_.get())));

  mock_task_runner_->RunUntilIdle();

  // Two delayed tasks with a run of 100 tasks, plus initial call.
  EXPECT_EQ(201, count);
  EXPECT_EQ(102, delayed_task_run_at_count);
}

TEST_F(WebViewSchedulerImplTest,
       MaxVirtualTimeTaskStarvationCountOneHundredNestedMessageLoop) {
  web_view_scheduler_->EnableVirtualTime();
  web_view_scheduler_->SetMaxVirtualTimeTaskStarvationCount(100);
  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  scheduler_->OnBeginNestedRunLoop();

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue(), 1000, &count);
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(DelayedTask, WTF::Unretained(&count),
                 WTF::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          WTF::Unretained(web_view_scheduler_.get())));

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(1000, count);
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

TEST_F(WebViewSchedulerImplTest, MaxVirtualTimeTaskStarvationCountZero) {
  web_view_scheduler_->EnableVirtualTime();
  web_view_scheduler_->SetMaxVirtualTimeTaskStarvationCount(0);
  web_view_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue(), 1000, &count);
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE,
      base::Bind(DelayedTask, WTF::Unretained(&count),
                 WTF::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  web_view_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      WTF::Bind(
          [](WebViewScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          WTF::Unretained(web_view_scheduler_.get())));

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(1000, count);
  // If the initial count had been higher, the delayed task could have been
  // arbitrarily delayed.
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

namespace {

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
      std::make_unique<base::FieldTrialList>(nullptr);
  InitializeTrialParams();
  web_view_scheduler_.reset(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));

  std::vector<base::TimeTicks> run_times;
  web_frame_scheduler_ = web_view_scheduler_->CreateWebFrameSchedulerImpl(
      nullptr, WebFrameScheduler::FrameType::kSubframe);
  web_view_scheduler_->SetPageVisible(true);

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(2500));

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &clock_, &run_times),
      TimeDelta::FromMilliseconds(1));
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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

  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &clock_, &run_times),
      TimeDelta::FromMicroseconds(1));
  ThrottleableTaskQueue()->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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
      std::make_unique<base::FieldTrialList>(nullptr);
  InitializeTrialParams();
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler(
      new WebViewSchedulerImpl(nullptr, nullptr, scheduler_.get(), false));

  std::vector<base::TimeTicks> run_times;

  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler1 =
      web_view_scheduler->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);
  std::unique_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler->CreateWebFrameSchedulerImpl(
          nullptr, WebFrameScheduler::FrameType::kSubframe);

  web_view_scheduler->SetPageVisible(false);

  // Wait for 20s to avoid initial throttling delay.
  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(20500));

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(web_frame_scheduler1.get())
        ->PostDelayedTask(FROM_HERE,
                          base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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
    ThrottleableTaskQueueForScheduler(web_frame_scheduler1.get())
        ->PostDelayedTask(FROM_HERE,
                          base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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
    ThrottleableTaskQueueForScheduler(web_frame_scheduler2.get())
        ->PostDelayedTask(FROM_HERE,
                          base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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
    ThrottleableTaskQueueForScheduler(web_frame_scheduler1.get())
        ->PostDelayedTask(FROM_HERE,
                          base::Bind(&ExpensiveTestTask, &clock_, &run_times),
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

}  // namespace web_view_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
