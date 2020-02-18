/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/repeating_task.h"

#include <atomic>
#include <chrono>  // Not allowed in production per Chromium style guide.
#include <memory>
#include <thread>  // Not allowed in production per Chromium style guide.

#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;

constexpr TimeDelta kTimeout = TimeDelta::Millis<1000>();

void Sleep(TimeDelta time_delta) {
  // Note that Chromium style guide prohibits use of <thread> and <chrono> in
  // production code, used here since webrtc::SleepMs may return early.
  std::this_thread::sleep_for(std::chrono::microseconds(time_delta.us()));
}

class MockClosure {
 public:
  MOCK_METHOD0(Call, TimeDelta());
  MOCK_METHOD0(Delete, void());
};

class MoveOnlyClosure {
 public:
  explicit MoveOnlyClosure(MockClosure* mock) : mock_(mock) {}
  MoveOnlyClosure(const MoveOnlyClosure&) = delete;
  MoveOnlyClosure(MoveOnlyClosure&& other) : mock_(other.mock_) {
    other.mock_ = nullptr;
  }
  ~MoveOnlyClosure() {
    if (mock_)
      mock_->Delete();
  }
  TimeDelta operator()() { return mock_->Call(); }

 private:
  MockClosure* mock_;
};
}  // namespace

TEST(RepeatingTaskTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(50);
  const TimeDelta kLongInterval = TimeDelta::ms(200);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;

  TaskQueueForTest task_queue("TestQueue");
  std::atomic_int counter(0);
  auto handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Sleep long enough to go through the initial phase.
  Sleep(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  // Sleep long enough that the task would run at least once more if not
  // stopped.
  Sleep(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(RepeatingTaskTest, CompensatesForLongRunTime) {
  const int kTargetCount = 20;
  const int kTargetCountMargin = 2;
  const TimeDelta kRepeatInterval = TimeDelta::ms(2);
  // Sleeping inside the task for longer than the repeat interval once, should
  // be compensated for by repeating the task faster to catch up.
  const TimeDelta kSleepDuration = TimeDelta::ms(20);
  const int kSleepAtCount = 3;

  std::atomic_int counter(0);
  TaskQueueForTest task_queue("TestQueue");
  RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    if (++counter == kSleepAtCount)
      Sleep(kSleepDuration);
    return kRepeatInterval;
  });
  Sleep(kRepeatInterval * kTargetCount);
  // Execution time should not have affected the run count,
  // but we allow some margin to reduce flakiness.
  EXPECT_GE(counter.load(), kTargetCount - kTargetCountMargin);
}

TEST(RepeatingTaskTest, CompensatesForShortRunTime) {
  std::atomic_int counter(0);
  TaskQueueForTest task_queue("TestQueue");
  RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    ++counter;
    // Sleeping for the 100 ms should be compensated.
    Sleep(TimeDelta::ms(100));
    return TimeDelta::ms(300);
  });
  Sleep(TimeDelta::ms(400));

  // We expect that the task have been called twice, once directly at Start and
  // once after 300 ms has passed.
  EXPECT_EQ(counter.load(), 2);
}

TEST(RepeatingTaskTest, CancelDelayedTaskBeforeItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).Times(0);
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));
  TaskQueueForTest task_queue("queue");
  auto handle = RepeatingTaskHandle::DelayedStart(
      task_queue.Get(), TimeDelta::ms(100), MoveOnlyClosure(&mock));
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, CancelTaskAfterItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).WillOnce(Return(TimeDelta::ms(100)));
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));
  TaskQueueForTest task_queue("queue");
  auto handle =
      RepeatingTaskHandle::Start(task_queue.Get(), MoveOnlyClosure(&mock));
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  TaskQueueForTest task_queue("TestQueue");
  RepeatingTaskHandle handle;
  task_queue.PostTask([&] {
    handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
      ++counter;
      handle.Stop();
      return TimeDelta::ms(2);
    });
  });
  Sleep(TimeDelta::ms(10));
  EXPECT_EQ(counter.load(), 1);
}

TEST(RepeatingTaskTest, ZeroReturnValueRepostsTheTask) {
  NiceMock<MockClosure> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(TimeDelta::Zero()))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeout;
      }));
  TaskQueueForTest task_queue("queue");
  RepeatingTaskHandle::Start(task_queue.Get(), MoveOnlyClosure(&closure));
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, StartPeriodicTask) {
  MockFunction<TimeDelta()> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeout;
      }));
  TaskQueueForTest task_queue("queue");
  RepeatingTaskHandle::Start(task_queue.Get(), closure.AsStdFunction());
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    TimeDelta TimeUntilNextRun() { return TimeDelta::ms(100); }
    void StartPeriodicTask(RepeatingTaskHandle* handle,
                           TaskQueueBase* task_queue) {
      *handle = RepeatingTaskHandle::Start(task_queue, [this] {
        DoPeriodicTask();
        return TimeUntilNextRun();
      });
    }
  };
  TaskQueueForTest task_queue("queue");
  auto object = std::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  RepeatingTaskHandle handle;
  object->StartPeriodicTask(&handle, task_queue.Get());
  // Restart the task
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  object->StartPeriodicTask(&handle, task_queue.Get());
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  struct Destructor {
    void operator()() { object.reset(); }
    std::unique_ptr<ObjectOnTaskQueue> object;
  };
  task_queue.PostTask(Destructor{std::move(object)});
  // Do not wait for the destructor closure in order to create a race between
  // task queue destruction and running the desctructor closure.
}

}  // namespace webrtc
