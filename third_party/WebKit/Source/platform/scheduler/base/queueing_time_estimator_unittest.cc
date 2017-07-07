// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/queueing_time_estimator.h"
#include "base/logging.h"
#include "platform/scheduler/base/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

using QueueingTimeEstimatorTest = ::testing::Test;

class TestQueueingTimeEstimatorClient : public QueueingTimeEstimator::Client {
 public:
  void OnQueueingTimeForWindowEstimated(
      base::TimeDelta queueing_time,
      base::TimeTicks window_start_time) override {
    expected_queueing_times_.push_back(queueing_time);
  }
  const std::vector<base::TimeDelta>& expected_queueing_times() {
    return expected_queueing_times_;
  }

 private:
  std::vector<base::TimeDelta> expected_queueing_times_;
};

class QueueingTimeEstimatorForTest : public QueueingTimeEstimator {
 public:
  QueueingTimeEstimatorForTest(TestQueueingTimeEstimatorClient* client,
                               base::TimeDelta window_duration,
                               int steps_per_window)
      : QueueingTimeEstimator(client, window_duration, steps_per_window) {}
};

// Three tasks of one second each, all within a 5 second window. Expected
// queueing time is the probability of falling into one of these tasks (3/5),
// multiplied by the expected queueing time within a task (0.5 seconds). Thus we
// expect a queueing time of 0.3 seconds.
TEST_F(QueueingTimeEstimatorTest, AllTasksWithinWindow) {
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  for (int i = 0; i < 3; ++i) {
    estimator.OnTopLevelTaskStarted(time);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
}

// One 20 second long task, starting 3 seconds into the first window.
// Window 1: Probability of being within task = 2/5. Expected delay within task:
// avg(20, 18). Total expected queueing time = 7.6s.
// Window 2: Probability of being within task = 1. Expected delay within task:
// avg(18, 13). Total expected queueing time = 15.5s.
// Window 5: Probability of being within task = 3/5. Expected delay within task:
// avg(3, 0). Total expected queueing time = 0.9s.
TEST_F(QueueingTimeEstimatorTest, MultiWindowTask) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                     base::TimeDelta::FromMilliseconds(15500),
                                     base::TimeDelta::FromMilliseconds(10500),
                                     base::TimeDelta::FromMilliseconds(5500),
                                     base::TimeDelta::FromMilliseconds(900)));
}

// The main thread is considered unresponsive during a single long task. In this
// case, the single long task is 3 seconds long.
// Probability of being with the task = 3/5. Expected delay within task:
// avg(0, 3). Total expected queueing time = 3/5 * 3/2 = 0.9s.
// In this example, the queueing time comes from the current, incomplete window.
TEST_F(QueueingTimeEstimatorTest,
       EstimateQueueingTimeDuringSingleLongTaskIncompleteWindow) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time);

  time += base::TimeDelta::FromMilliseconds(3000);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(900), estimated_queueing_time);
}

// The main thread is considered unresponsive during a single long task, which
// exceeds the size of one window. We report the queueing time of the most
// recent window. Probability of being within the task = 100%, as the task
// fills the whole window. Expected delay within this task = avg(8, 3) = 5.5.
TEST_F(QueueingTimeEstimatorTest,
       EstimateQueueingTimeDuringSingleLongTaskExceedingWindow) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time);

  time += base::TimeDelta::FromMilliseconds(13000);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5500), estimated_queueing_time);
}
//                         Estimate
//                           |
//                           v
// Task|------------------------------...
// Time|---o---o---o---o---o---o-------->
//     0   1   2   3   4   5   6
//     | s | s | s | s | s |
//     |--------win1-------|
//         |--------win2-------|
//
// s: step window
// win1: The last full window.
// win2: The partial window.
//
// EQT(win1) = (0.5 + 5.5) / 2 * (5 / 5) = 3
// EQT(win2) = (4.5 + 0) / 2 * (4.5 / 5) = 2.025
// So EQT = max(EQT(win1), EQT(win2)) = 3
TEST_F(QueueingTimeEstimatorTest,
       SlidingWindowEstimateQueueingTimeFullWindowLargerThanPartial) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time);

  time += base::TimeDelta::FromMilliseconds(5500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3000), estimated_queueing_time);
}
//                        Estimate
//                           |
//                           v
// Task                    |----------...
// Time|---o---o---o---o---o---o-------->
//     0   1   2   3   4   5   6
//     | s | s | s | s | s |
//     |--------win1-------|
//         |--------win2-------|
//
// s: step window
// win1: The last full window.
// win2: The partial window.
//
// EQT(win1) = 0
// EQT(win2) = (0 + 0.5) / 2 * (0.5 / 2) = 0.025
// So EQT = max(EQT(win1), EQT(win2)) = 0.025
TEST_F(QueueingTimeEstimatorTest,
       SlidingWindowEstimateQueueingTimePartialWindowLargerThanFull) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(5000);
  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time);
  time += base::TimeDelta::FromMilliseconds(500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(25), estimated_queueing_time);
}

// Tasks containing nested run loops may be extremely long without
// negatively impacting user experience. Ignore such tasks.
TEST_F(QueueingTimeEstimatorTest, IgnoresTasksWithNestedMessageLoops) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnBeginNestedRunLoop();
  estimator.OnTopLevelTaskCompleted(time);

  // Perform an additional task after the nested run loop. A 1 second task
  // in a 5 second window results in a 100ms expected queueing time.
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100)));
}

// If a task is too long, we assume it's invalid. Perhaps the user's machine
// went to sleep during a task, resulting in an extremely long task. Ignore
// these long tasks completely.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongTasks) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  // Start with a 1 second task.
  base::TimeTicks time;
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Now perform an invalid task.
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnTopLevelTaskCompleted(time);

  // Perform another 1 second task.
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100)));
}

// ^ Instantaneous queuing time
// |
// |
// |   |\                                          .
// |   |  \                                        .
// |   |    \                                      .
// |   |      \                                    .
// |   |        \             |                    .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest, SlidingWindowOverOneTask) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(1600),
                                     base::TimeDelta::FromMilliseconds(2100),
                                     base::TimeDelta::FromMilliseconds(2400),
                                     base::TimeDelta::FromMilliseconds(2500),
                                     base::TimeDelta::FromMilliseconds(1600),
                                     base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(0)));
}

// ^ Instantaneous queuing time
// |
// |
// |   |\                                            .
// |   | \                                           .
// |   |  \                                          .
// |   |   \  |\                                     .
// |   |    \ | \           |                        .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest, SlidingWindowOverTwoTasksWithinFirstWindow) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(500);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  std::vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(600),
      base::TimeDelta::FromMilliseconds(625),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(325),
      base::TimeDelta::FromMilliseconds(125),
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(0)};
  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAreArray(expected_durations));
}

// ^ Instantaneous queuing time
// |
// |
// |           |\                                 .
// |           | \                                .
// |           |  \                               .
// |           |   \ |\                           .
// |           |    \| \           |              .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest,
       SlidingWindowOverTwoTasksSpanningSeveralWindows) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(4000);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time);
  estimator.OnTopLevelTaskCompleted(time);

  std::vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(600),
      base::TimeDelta::FromMilliseconds(700),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(325),
      base::TimeDelta::FromMilliseconds(125),
      base::TimeDelta::FromMilliseconds(25),
      base::TimeDelta::FromMilliseconds(0)};

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAreArray(expected_durations));
}

}  // namespace scheduler
}  // namespace blink
