// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/webrtc_timer.h"

#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

namespace {

class WebRtcTimerTest : public ::testing::Test {
 public:
  WebRtcTimerTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Ensure mock time is aligned with metronome tick.
    base::TimeTicks now = base::TimeTicks::Now();
    task_environment_.FastForwardBy(
        MetronomeSource::TimeSnappedToNextTick(now) - now);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

class CallbackListener {
 public:
  CallbackListener()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

  void Callback() {
    EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    ++callback_count_;
  }

  void set_task_runner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    task_runner_ = task_runner;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }
  size_t callback_count() const { return callback_count_; }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  size_t callback_count_ = 0u;
};

class RecursiveStartOneShotter {
 public:
  RecursiveStartOneShotter(size_t repeat_count, base::TimeDelta delay)
      : timer_(base::ThreadPool::CreateSequencedTaskRunner({}),
               base::BindRepeating(&RecursiveStartOneShotter::Callback,
                                   base::Unretained(this))),
        repeat_count_(repeat_count),
        delay_(delay) {
    timer_.StartOneShot(delay_);
  }
  ~RecursiveStartOneShotter() { timer_.Shutdown(); }

  size_t callback_count() const { return callback_count_; }

  void Callback() {
    ++callback_count_;
    DCHECK(repeat_count_);
    --repeat_count_;
    if (repeat_count_) {
      timer_.StartOneShot(delay_);
    }
  }

 private:
  WebRtcTimer timer_;
  size_t repeat_count_;
  base::TimeDelta delay_;
  size_t callback_count_ = 0u;
};

class RecursiveStopper {
 public:
  explicit RecursiveStopper(base::TimeDelta delay)
      : timer_(base::ThreadPool::CreateSequencedTaskRunner({}),
               base::BindRepeating(&RecursiveStopper::Callback,
                                   base::Unretained(this))) {
    timer_.StartRepeating(delay);
  }
  ~RecursiveStopper() { timer_.Shutdown(); }

  size_t callback_count() const { return callback_count_; }

  void Callback() {
    ++callback_count_;
    timer_.Stop();
  }

 private:
  WebRtcTimer timer_;
  size_t callback_count_ = 0u;
};

class IsActiveChecker {
 public:
  IsActiveChecker()
      : timer_(base::ThreadPool::CreateSequencedTaskRunner({}),
               base::BindRepeating(&IsActiveChecker::Callback,
                                   base::Unretained(this))) {}
  ~IsActiveChecker() { timer_.Shutdown(); }

  WebRtcTimer& timer() { return timer_; }
  bool was_active_in_last_callback() const {
    return was_active_in_last_callback_;
  }

  void Callback() { was_active_in_last_callback_ = timer_.IsActive(); }

 private:
  WebRtcTimer timer_;
  bool was_active_in_last_callback_;
};

}  // namespace

TEST_F(WebRtcTimerTest, StartOneShotWithoutMetronome) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Ensure the timer fires on the target millisecond.
  timer.StartOneShot(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(9));
  EXPECT_EQ(listener.callback_count(), 0u);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(listener.callback_count(), 1u);

  // The task does not repeat automatically.
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 1u);

  // The task can be manually scheduled to fire again.
  timer.StartOneShot(base::Milliseconds(5));
  task_environment_.FastForwardBy(base::Milliseconds(5));
  EXPECT_EQ(listener.callback_count(), 2u);

  // Schedule to fire but shutdown the timer before it has time to fire.
  timer.StartOneShot(base::Milliseconds(5));
  timer.Shutdown();

  task_environment_.FastForwardBy(base::Milliseconds(5));
  EXPECT_EQ(listener.callback_count(), 2u);
}

TEST_F(WebRtcTimerTest, StartOneShotWithMetronome) {
  base::test::ScopedFeatureList scoped_feature_list(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Schedule to fire on the first tick.
  timer.StartOneShot(MetronomeSource::Tick());
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 1u);

  // The task does not repeat automatically.
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 1u);

  // Schedule to fire a millisecond before the next tick. Advancing to that
  // time does not result in a callback.
  timer.StartOneShot(MetronomeSource::Tick() - base::Milliseconds(1));
  task_environment_.FastForwardBy(MetronomeSource::Tick() -
                                  base::Milliseconds(1));
  EXPECT_EQ(listener.callback_count(), 1u);
  // But it fires on the next tick.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(listener.callback_count(), 2u);

  // Fire a little after the next tick. Two ticks has to pass before anything
  // happens.
  timer.StartOneShot(MetronomeSource::Tick() + base::Milliseconds(1));
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 2u);
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 3u);

  // Schedule to fire but shutdown the timer before it has time to fire.
  timer.StartOneShot(MetronomeSource::Tick());
  timer.Shutdown();

  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 3u);
}

TEST_F(WebRtcTimerTest, RecursiveStartOneShotWithoutMetronome) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  base::TimeDelta delay = base::Milliseconds(1);
  RecursiveStartOneShotter recursive_shotter(/*repeat_count=*/2, delay);

  // Ensure the callback is repeated twice.
  EXPECT_EQ(recursive_shotter.callback_count(), 0u);
  task_environment_.FastForwardBy(delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 1u);
  task_environment_.FastForwardBy(delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 2u);
  // It is not repeated a third time.
  task_environment_.FastForwardBy(delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 2u);
}

TEST_F(WebRtcTimerTest, RecursiveStartOneShotWithMetronome) {
  base::test::ScopedFeatureList scoped_feature_list(kWebRtcTimerUsesMetronome);

  base::TimeDelta delay = base::Milliseconds(1);
  RecursiveStartOneShotter recursive_shotter(/*repeat_count=*/2, delay);

  // A full tick is needed before the callback fires.
  task_environment_.FastForwardBy(delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 0u);
  task_environment_.FastForwardBy(MetronomeSource::Tick() - delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 1u);

  // The same is true the second time it fires. This is not a high precision
  // timer and no attempt is taken to fire the callback multiple times per tick
  // to "catch up" with what the callback count would have been if the timer had
  // higher precision.
  task_environment_.FastForwardBy(delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 1u);
  task_environment_.FastForwardBy(MetronomeSource::Tick() - delay);
  EXPECT_EQ(recursive_shotter.callback_count(), 2u);

  // It is not repeated a third time.
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(recursive_shotter.callback_count(), 2u);
}

TEST_F(WebRtcTimerTest, MoveToNewTaskRunnerWithoutMetronome) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Schedule in less than a tick, and advance time half-way there.
  timer.StartOneShot(base::Milliseconds(6));
  task_environment_.FastForwardBy(base::Milliseconds(3));
  EXPECT_EQ(listener.callback_count(), 0u);

  // Move to a new task runner. The CallbackListener will EXPECT_TRUE that the
  // correct task runner is used.
  listener.set_task_runner(base::ThreadPool::CreateSequencedTaskRunner({}));
  timer.MoveToNewTaskRunner(listener.task_runner());

  // Advance to scheduled time.
  task_environment_.FastForwardBy(base::Milliseconds(3));
  EXPECT_EQ(listener.callback_count(), 1u);

  // Cleanup.
  timer.Shutdown();
}

TEST_F(WebRtcTimerTest, MoveToNewTaskRunnerWithMetronome) {
  base::test::ScopedFeatureList scoped_feature_list(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Schedule on the next tick, and advance time close to that.
  timer.StartOneShot(MetronomeSource::Tick());
  task_environment_.FastForwardBy(MetronomeSource::Tick() -
                                  base::Milliseconds(3));
  EXPECT_EQ(listener.callback_count(), 0u);

  // Move to a new task runner. The CallbackListener will EXPECT_TRUE that the
  // correct task runner is used.
  listener.set_task_runner(base::ThreadPool::CreateSequencedTaskRunner({}));
  timer.MoveToNewTaskRunner(listener.task_runner());

  // Advance to scheduled time (the next tick).
  task_environment_.FastForwardBy(base::Milliseconds(3));
  EXPECT_EQ(listener.callback_count(), 1u);

  // Cleanup.
  timer.Shutdown();
}

TEST_F(WebRtcTimerTest, StartRepeatingWithoutMetronome) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Ensure the timer repeats every 10 milliseconds.
  timer.StartRepeating(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 2u);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 3u);
  timer.Shutdown();

  // The timer stops on shutdown.
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 3u);
}

TEST_F(WebRtcTimerTest, StartRepeatingWithMetronome) {
  base::test::ScopedFeatureList scoped_feature_list(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // The timer can only fire on ticks, so 10 milliseconds is not enough here.
  timer.StartRepeating(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 0u);
  // But it does repeat on every tick.
  task_environment_.FastForwardBy(MetronomeSource::Tick() -
                                  base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 1u);
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 2u);
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 3u);
  timer.Shutdown();

  // The timer stops on shutdown.
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(listener.callback_count(), 3u);
}

TEST_F(WebRtcTimerTest, StopRepeatingTimer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  // Repeat every 10 ms.
  timer.StartRepeating(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 2u);

  // Stop the timer and ensure it stops repeating.
  timer.Stop();
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 2u);

  // The timer is reusable - can start and stop again.
  timer.StartRepeating(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 3u);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 4u);
  timer.Stop();
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(listener.callback_count(), 4u);

  // Cleanup.
  timer.Shutdown();
}

TEST_F(WebRtcTimerTest, StopTimerFromInsideCallbackWithoutMetronome) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  // Stops its own timer from inside the callback after 10 ms.
  RecursiveStopper recursive_stopper(base::Milliseconds(10));
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(recursive_stopper.callback_count(), 1u);

  // Ensure we are stopped, the callback count does not increase.
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(recursive_stopper.callback_count(), 1u);
}

// Ensures stopping inside the timer callback does not deadlock.
TEST_F(WebRtcTimerTest, StopTimerFromInsideCallbackWithMetronome) {
  base::test::ScopedFeatureList scoped_feature_list(kWebRtcTimerUsesMetronome);

  // Stops its own timer from inside the callback after a tick.
  RecursiveStopper recursive_stopper(MetronomeSource::Tick());
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(recursive_stopper.callback_count(), 1u);

  // Ensure we are stopped, the callback count does not increase.
  task_environment_.FastForwardBy(MetronomeSource::Tick());
  EXPECT_EQ(recursive_stopper.callback_count(), 1u);
}

// Ensures in-parallel stopping while the task may be running does not
// deadlock in race condition. Coverage for https://crbug.com/1281399.
TEST(WebRtcTimerRealThreadsTest, StopTimerWithRaceCondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME);

  CallbackListener listener;
  WebRtcTimer timer(listener.task_runner(),
                    base::BindRepeating(&CallbackListener::Callback,
                                        base::Unretained(&listener)));

  scoped_refptr<base::SequencedTaskRunner> dedicated_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  // Create a race condition between running the timer's task and stopping the
  // timer.
  timer.StartOneShot(base::Milliseconds(0));
  base::WaitableEvent event;
  dedicated_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](WebRtcTimer* timer, base::WaitableEvent* event) {
                       timer->Stop();
                       event->Signal();
                     },
                     base::Unretained(&timer), base::Unretained(&event)));
  event.Wait();

  timer.Shutdown();
}

TEST_F(WebRtcTimerTest, IsActive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kWebRtcTimerUsesMetronome);

  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  IsActiveChecker is_active_checker;

  // StartOneShot() makes the timer temporarily active.
  EXPECT_FALSE(is_active_checker.timer().IsActive());
  is_active_checker.timer().StartOneShot(kDelay);
  EXPECT_TRUE(is_active_checker.timer().IsActive());
  task_environment_.FastForwardBy(kDelay);
  EXPECT_FALSE(is_active_checker.timer().IsActive());
  // The timer is said to be inactive inside the one-shot callback.
  EXPECT_FALSE(is_active_checker.was_active_in_last_callback());

  // StartRepeating() makes the timer active until stopped.
  EXPECT_FALSE(is_active_checker.timer().IsActive());
  is_active_checker.timer().StartRepeating(kDelay);
  EXPECT_TRUE(is_active_checker.timer().IsActive());
  task_environment_.FastForwardBy(kDelay);
  EXPECT_TRUE(is_active_checker.timer().IsActive());
  // The timer is said to be active inside the repeating callback.
  EXPECT_TRUE(is_active_checker.was_active_in_last_callback());
}

}  // namespace blink
