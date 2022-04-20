// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr TimeDelta kTestDelay = Seconds(10);
constexpr TimeDelta kLongTestDelay = Minutes(10);

// The main thread types on which each timer should be tested.
const test::TaskEnvironment::MainThreadType testing_main_threads[] = {
    test::TaskEnvironment::MainThreadType::DEFAULT,
    test::TaskEnvironment::MainThreadType::IO,
#if !BUILDFLAG(IS_IOS)  // iOS does not allow direct running of the UI loop.
    test::TaskEnvironment::MainThreadType::UI,
#endif
};

class Receiver {
 public:
  Receiver() : count_(0) {}
  void OnCalled() { count_++; }
  bool WasCalled() { return count_ > 0; }
  int TimesCalled() { return count_; }

 private:
  int count_;
};

// Basic test with same setup as RunTest_OneShotTimers_Cancel below to confirm
// that |timer| would be fired in that test if it wasn't for the deletion.
void RunTest_OneShotTimers(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  OneShotTimer timer;
  timer.Start(FROM_HERE, kTestDelay,
              BindOnce(&Receiver::OnCalled, Unretained(&receiver)));

  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

void RunTest_OneShotTimers_Cancel(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  auto timer = std::make_unique<OneShotTimer>();
  auto* timer_ptr = timer.get();

  // This should run before the timer expires.
  SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, std::move(timer));

  timer_ptr->Start(FROM_HERE, kTestDelay,
                   BindOnce(&Receiver::OnCalled, Unretained(&receiver)));

  task_environment.FastForwardBy(kTestDelay);
  EXPECT_FALSE(receiver.WasCalled());
}

void RunTest_OneShotSelfDeletingTimer(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  auto timer = std::make_unique<OneShotTimer>();
  auto* timer_ptr = timer.get();

  timer_ptr->Start(
      FROM_HERE, kTestDelay,
      BindLambdaForTesting([&receiver, timer = std::move(timer)]() mutable {
        receiver.OnCalled();
        timer.reset();
      }));

  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

void RunTest_RepeatingTimer(
    test::TaskEnvironment::MainThreadType main_thread_type,
    const TimeDelta& delay) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  RepeatingTimer timer;
  timer.Start(FROM_HERE, kTestDelay,
              BindRepeating(&Receiver::OnCalled, Unretained(&receiver)));

  task_environment.FastForwardBy(20 * kTestDelay);
  EXPECT_EQ(receiver.TimesCalled(), 20);
}

void RunTest_RepeatingTimer_Cancel(
    test::TaskEnvironment::MainThreadType main_thread_type,
    const TimeDelta& delay) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  auto timer = std::make_unique<RepeatingTimer>();
  auto* timer_ptr = timer.get();

  // This should run before the timer expires.
  SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, std::move(timer));

  timer_ptr->Start(FROM_HERE, delay,
                   BindRepeating(&Receiver::OnCalled, Unretained(&receiver)));

  task_environment.FastForwardBy(delay);
  EXPECT_FALSE(receiver.WasCalled());
}

void RunTest_DelayTimer_NoCall(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  DelayTimer timer(FROM_HERE, kTestDelay, &receiver, &Receiver::OnCalled);

  task_environment.FastForwardBy(kTestDelay);
  EXPECT_FALSE(receiver.WasCalled());
}

void RunTest_DelayTimer_OneCall(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  DelayTimer timer(FROM_HERE, kTestDelay, &receiver, &Receiver::OnCalled);
  timer.Reset();

  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

void RunTest_DelayTimer_Reset(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;
  DelayTimer timer(FROM_HERE, kTestDelay, &receiver, &Receiver::OnCalled);
  timer.Reset();

  // Fast-forward by a delay smaller than the timer delay. The timer will not
  // fire.
  task_environment.FastForwardBy(kTestDelay / 2);
  EXPECT_FALSE(receiver.WasCalled());

  // Postpone the fire time.
  timer.Reset();

  // Verify that the timer does not fire at its original fire time.
  task_environment.FastForwardBy(kTestDelay / 2);
  EXPECT_FALSE(receiver.WasCalled());

  // Fast-forward by the timer delay. The timer will fire.
  task_environment.FastForwardBy(kTestDelay / 2);
  EXPECT_TRUE(receiver.WasCalled());
}

void RunTest_DelayTimer_Deleted(
    test::TaskEnvironment::MainThreadType main_thread_type) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME, main_thread_type);

  Receiver receiver;

  {
    DelayTimer timer(FROM_HERE, kTestDelay, &receiver, &Receiver::OnCalled);
    timer.Reset();
  }

  // Because the timer was deleted, it will never fire.
  task_environment.FastForwardBy(kTestDelay);
  EXPECT_FALSE(receiver.WasCalled());
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of main thread.  That way we are sure
// that timers work properly in all configurations.

class TimerTestWithThreadType
    : public testing::TestWithParam<test::TaskEnvironment::MainThreadType> {};

TEST_P(TimerTestWithThreadType, OneShotTimers) {
  RunTest_OneShotTimers(GetParam());
}

TEST_P(TimerTestWithThreadType, OneShotTimers_Cancel) {
  RunTest_OneShotTimers_Cancel(GetParam());
}

// If underline timer does not handle properly, we will crash or fail
// in full page heap environment.
TEST_P(TimerTestWithThreadType, OneShotSelfDeletingTimer) {
  RunTest_OneShotSelfDeletingTimer(GetParam());
}

TEST(TimerTest, OneShotTimer_CustomTaskRunner) {
  auto task_runner = base::MakeRefCounted<TestSimpleTaskRunner>();

  OneShotTimer timer;

  bool task_ran = false;

  // The timer will use the TestSimpleTaskRunner to schedule its delays.
  timer.SetTaskRunner(task_runner);
  timer.Start(FROM_HERE, Days(1),
              BindLambdaForTesting([&]() { task_ran = true; }));

  EXPECT_FALSE(task_ran);
  EXPECT_TRUE(task_runner->HasPendingTask());

  task_runner->RunPendingTasks();

  EXPECT_TRUE(task_ran);
}

TEST(TimerTest, OneShotTimerWithTickClock) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  Receiver receiver;
  OneShotTimer timer(task_environment.GetMockTickClock());
  timer.Start(FROM_HERE, kTestDelay,
              BindOnce(&Receiver::OnCalled, Unretained(&receiver)));
  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

TEST_P(TimerTestWithThreadType, RepeatingTimer) {
  RunTest_RepeatingTimer(GetParam(), kTestDelay);
}

TEST_P(TimerTestWithThreadType, RepeatingTimer_Cancel) {
  RunTest_RepeatingTimer_Cancel(GetParam(), kTestDelay);
}

TEST_P(TimerTestWithThreadType, RepeatingTimerZeroDelay) {
  RunTest_RepeatingTimer(GetParam(), Seconds(0));
}

TEST_P(TimerTestWithThreadType, RepeatingTimerZeroDelay_Cancel) {
  RunTest_RepeatingTimer_Cancel(GetParam(), Seconds(0));
}

TEST(TimerTest, RepeatingTimerWithTickClock) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  Receiver receiver;
  const int expected_times_called = 10;
  RepeatingTimer timer(task_environment.GetMockTickClock());
  timer.Start(FROM_HERE, kTestDelay,
              BindRepeating(&Receiver::OnCalled, Unretained(&receiver)));
  task_environment.FastForwardBy(expected_times_called * kTestDelay);
  timer.Stop();
  EXPECT_EQ(expected_times_called, receiver.TimesCalled());
}

TEST_P(TimerTestWithThreadType, DelayTimer_NoCall) {
  RunTest_DelayTimer_NoCall(GetParam());
}

TEST_P(TimerTestWithThreadType, DelayTimer_OneCall) {
  RunTest_DelayTimer_OneCall(GetParam());
}

TEST_P(TimerTestWithThreadType, DelayTimer_Reset) {
  RunTest_DelayTimer_Reset(GetParam());
}

TEST_P(TimerTestWithThreadType, DelayTimer_Deleted) {
  RunTest_DelayTimer_Deleted(GetParam());
}

TEST(TimerTest, DelayTimerWithTickClock) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  Receiver receiver;
  DelayTimer timer(FROM_HERE, kTestDelay, &receiver, &Receiver::OnCalled,
                   task_environment.GetMockTickClock());
  task_environment.FastForwardBy(kTestDelay - Microseconds(1));
  EXPECT_FALSE(receiver.WasCalled());
  timer.Reset();
  task_environment.FastForwardBy(kTestDelay - Microseconds(1));
  EXPECT_FALSE(receiver.WasCalled());
  timer.Reset();
  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

TEST(TimerTest, TaskEnvironmentShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking is enabled.
  Receiver receiver;
  OneShotTimer timer;

  {
    test::TaskEnvironment task_environment;
    timer.Start(FROM_HERE, kTestDelay,
                BindOnce(&Receiver::OnCalled, Unretained(&receiver)));
  }  // Task environment destructs by falling out of scope.

  EXPECT_FALSE(receiver.WasCalled());
  // Timer destruct. SHOULD NOT CRASH, of course.
}

TEST(TimerTest, TaskEnvironmentShutdownSelfOwningTimer) {
  // This test verifies that shutdown of the task environment does not cause
  // crashes if there is a pending timer not yet fired and |Timer::user_task_|
  // owns the timer. The test may only trigger exceptions if debug heap checking
  // is enabled.

  auto timer = std::make_unique<OneShotTimer>();
  auto* timer_ptr = timer.get();

  test::TaskEnvironment task_environment;

  timer_ptr->Start(FROM_HERE, kTestDelay,
                   BindLambdaForTesting([timer = std::move(timer)]() {}));
  // |Timer::user_task_| owns sole reference to |timer|.

  // Task environment destructs by falling out of scope. SHOULD NOT CRASH.
}

TEST(TimerTest, NonRepeatIsRunning) {
  {
    test::TaskEnvironment task_environment;
    OneShotTimer timer;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, kTestDelay, DoNothing());
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
  }

  {
    RetainingOneShotTimer timer;
    test::TaskEnvironment task_environment;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, kTestDelay, DoNothing());
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
    ASSERT_FALSE(timer.user_task().is_null());
    timer.Reset();
    EXPECT_TRUE(timer.IsRunning());
  }
}

TEST(TimerTest, NonRepeatTaskEnvironmentDeath) {
  OneShotTimer timer;
  {
    test::TaskEnvironment task_environment;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, kTestDelay, DoNothing());
    EXPECT_TRUE(timer.IsRunning());
  }
  EXPECT_FALSE(timer.IsRunning());
}

TEST(TimerTest, RetainRepeatIsRunning) {
  test::TaskEnvironment task_environment;
  RepeatingTimer timer(FROM_HERE, kTestDelay, DoNothing());
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

TEST(TimerTest, RetainNonRepeatIsRunning) {
  test::TaskEnvironment task_environment;
  RetainingOneShotTimer timer(FROM_HERE, kTestDelay, DoNothing());
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

//-----------------------------------------------------------------------------

TEST(TimerTest, ContinuationStopStart) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);

  Receiver receiver1;
  Receiver receiver2;
  OneShotTimer timer;
  timer.Start(FROM_HERE, kTestDelay,
              BindOnce(&Receiver::OnCalled, Unretained(&receiver1)));
  timer.Stop();
  timer.Start(FROM_HERE, kLongTestDelay,
              BindOnce(&Receiver::OnCalled, Unretained(&receiver2)));
  task_environment.FastForwardBy(kLongTestDelay);
  EXPECT_FALSE(receiver1.WasCalled());
  EXPECT_TRUE(receiver2.WasCalled());
}

TEST(TimerTest, ContinuationReset) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);

  Receiver receiver;
  OneShotTimer timer;
  timer.Start(FROM_HERE, kTestDelay,
              BindOnce(&Receiver::OnCalled, Unretained(&receiver)));
  timer.Reset();
  // // Since Reset happened before task ran, the user_task must not be
  // cleared: ASSERT_FALSE(timer.user_task().is_null());
  task_environment.FastForwardBy(kTestDelay);
  EXPECT_TRUE(receiver.WasCalled());
}

TEST(TimerTest, AbandonedTaskIsCancelled) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  OneShotTimer timer;

  // Start a timer. There will be a pending task on the current sequence.
  timer.Start(FROM_HERE, kTestDelay, base::DoNothing());
  EXPECT_EQ(1u, task_environment.GetPendingMainThreadTaskCount());

  // After AbandonAndStop(), the task is correctly treated as cancelled.
  timer.AbandonAndStop();
  EXPECT_EQ(0u, task_environment.GetPendingMainThreadTaskCount());
}

TEST(TimerTest, DeadlineTimer) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  RunLoop run_loop;
  DeadlineTimer timer;
  TimeTicks start = TimeTicks::Now();

  timer.Start(FROM_HERE, start + Seconds(5), run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(start + Seconds(5), TimeTicks::Now());
}

TEST(TimerTest, DeadlineTimerCancel) {
  test::TaskEnvironment task_environment(
      test::TaskEnvironment::TimeSource::MOCK_TIME);
  RunLoop run_loop;
  DeadlineTimer timer;
  TimeTicks start = TimeTicks::Now();

  MockRepeatingCallback<void()> callback;
  timer.Start(FROM_HERE, start + Seconds(5), callback.Get());

  EXPECT_CALL(callback, Run()).Times(0);
  timer.Stop();
  task_environment.FastForwardBy(Seconds(5));
  EXPECT_EQ(start + Seconds(5), TimeTicks::Now());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TimerTestWithThreadType,
                         testing::ValuesIn(testing_main_threads));

}  // namespace base
