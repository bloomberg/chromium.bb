// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/queueing_time_estimator.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/testing/HistogramTester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <map>
#include <string>
#include <vector>

namespace blink {
namespace scheduler {

using QueueingTimeEstimatorTest = ::testing::Test;
using QueueType = MainThreadTaskQueue::QueueType;

const QueueType kDefaultQueueType = QueueType::OTHER;

class TestQueueingTimeEstimatorClient : public QueueingTimeEstimator::Client {
 public:
  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override {
    expected_queueing_times_.push_back(queueing_time);
    // Mimic RendererSchedulerImpl::OnQueueingTimeForWindowEstimated.
    if (is_disjoint_window) {
      UMA_HISTOGRAM_TIMES("RendererScheduler.ExpectedTaskQueueingDuration",
                          queueing_time);
    }
  }
  void OnReportSplitExpectedQueueingTime(
      const std::string& split_description,
      base::TimeDelta queueing_time) override {
    if (split_eqts_.find(split_description) == split_eqts_.end())
      split_eqts_[split_description] = std::vector<base::TimeDelta>();
    split_eqts_[split_description].push_back(queueing_time);
    // Mimic RendererSchedulerImpl::OnReportSplitExpectedQueueingTime.
    base::UmaHistogramTimes(split_description, queueing_time);
  }
  const std::vector<base::TimeDelta>& expected_queueing_times() {
    return expected_queueing_times_;
  }
  const std::map<std::string, std::vector<base::TimeDelta>>& split_eqts() {
    return split_eqts_;
  }
  const std::vector<base::TimeDelta>& GetValueFor(QueueType queue_type) {
    return split_eqts_[QueueingTimeEstimator::Calculator::
                           GetReportingMessageFromQueueType(queue_type)];
  }

 private:
  std::vector<base::TimeDelta> expected_queueing_times_;
  std::map<std::string, std::vector<base::TimeDelta>> split_eqts_;
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
  HistogramTester histogram_tester;
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  for (int i = 0; i < 3; ++i) {
    estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 300, 1);
}

// One 20 second long task, starting 3 seconds into the first window.
// Window 1: Probability of being within task = 2/5. Expected delay within task:
// avg(20, 18). Total expected queueing time = 7.6s.
// Window 2: Probability of being within task = 1. Expected delay within task:
// avg(18, 13). Total expected queueing time = 15.5s.
// Window 5: Probability of being within task = 3/5. Expected delay within task:
// avg(3, 0). Total expected queueing time = 0.9s.
TEST_F(QueueingTimeEstimatorTest, MultiWindowTask) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                     base::TimeDelta::FromMilliseconds(15500),
                                     base::TimeDelta::FromMilliseconds(10500),
                                     base::TimeDelta::FromMilliseconds(5500),
                                     base::TimeDelta::FromMilliseconds(900)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 900, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 5500, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 7600, 1);
  // 10500 and 15500 are in the same bucket of 10000+.
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 10500, 2);
}

// The main thread is considered unresponsive during a single long task. In this
// case, the single long task is 3 seconds long.
// Probability of being with the task = 3/5. Expected delay within task:
// avg(0, 3). Total expected queueing time = 3/5 * 3/2 = 0.9s.
// In this example, the queueing time comes from the current, incomplete window.
TEST_F(QueueingTimeEstimatorTest,
       EstimateQueueingTimeDuringSingleLongTaskIncompleteWindow) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, kDefaultQueueType);

  time += base::TimeDelta::FromMilliseconds(3000);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(900), estimated_queueing_time);
  // Window time was not completed, so no UMA should be recorded.
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0);
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
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, kDefaultQueueType);

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
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, kDefaultQueueType);

  time += base::TimeDelta::FromMilliseconds(5500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3000), estimated_queueing_time);
  // Only EstimateQueueingTimeIncludingCurrentTask has been called after window
  // completion, so UMA should not have been reported yet.
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0);
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
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(5000);
  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(25), estimated_queueing_time);
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0, 1);
}

// Tasks containing nested run loops may be extremely long without
// negatively impacting user experience. Ignore such tasks.
TEST_F(QueueingTimeEstimatorTest, IgnoresTasksWithNestedMessageLoops) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(5000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnBeginNestedRunLoop();
  estimator.OnTopLevelTaskCompleted(time);

  // Perform an additional task after the nested run loop. A 1 second task
  // in a 5 second window results in a 100ms expected queueing time.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 100, 1);
}

// If a task is too long, we assume it's invalid. Perhaps the user's machine
// went to sleep during a task, resulting in an extremely long task. Ignore
// these long tasks completely.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongTasks) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(4000);

  // Now perform an invalid task. This will cause the windows involving this
  // task to be ignored.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnTopLevelTaskCompleted(time);

  // Perform another 1 second task.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  // Now perform another invalid task. This will cause the windows involving
  // this task to be ignored. Therefore, the previous task is ignored.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(100)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 100, 2);
}

// If we idle for too long, ignore idling time, even if the renderer is on the
// foreground. Perhaps the user's machine went to sleep while we were idling.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongIdlePeriods) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(4000);
  // Dummy task to ensure this window is reported.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  // Now go idle for long. This will cause the windows involving this
  // time to be ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Perform another 1 second task.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  // Now go idle again. This will cause the windows involving this idle period
  // to be ignored. Therefore, the previous task is ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(100)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 100, 2);
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
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  std::vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(900),
      base::TimeDelta::FromMilliseconds(1600),
      base::TimeDelta::FromMilliseconds(2100),
      base::TimeDelta::FromMilliseconds(2400),
      base::TimeDelta::FromMilliseconds(2500),
      base::TimeDelta::FromMilliseconds(1600),
      base::TimeDelta::FromMilliseconds(900),
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0)};
  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAreArray(expected_durations));
  // UMA reported only on disjoint windows.
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2500, 1);
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
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(500);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
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
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0)};
  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAreArray(expected_durations));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 725, 1);
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
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(4000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
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
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 325, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 400, 1);
}

// There are multiple windows, but some of the EQTs are not reported due to
// backgrounded renderer. EQT(win1) = 0. EQT(win3) = (1500+500)/2 = 1000.
// EQT(win4) = 1/2*500/2 = 250. EQT(win7) = 1/5*200/2 = 20.
TEST_F(QueueingTimeEstimatorTest, BackgroundedEQTsWithSingleStepPerWindow) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(1), 1);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(1001);

  // Second window should not be reported.
  estimator.OnRendererStateChanged(true, time);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(456);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRendererStateChanged(false, time);
  time += base::TimeDelta::FromMilliseconds(343);

  // Third, fourth windows should be reported
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(501);

  // Fifth, sixth task should not be reported
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRendererStateChanged(false, time);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(999);

  // Seventh task should be reported.
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(1000),
                                     base::TimeDelta::FromMilliseconds(125),
                                     base::TimeDelta::FromMilliseconds(20)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 4);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 20, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 125, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 1000, 1);
}

// We only ignore steps that contain some part that is backgrounded. Thus a
// window could be made up of non-contiguous steps. The following are EQTs, with
// time deltas with respect to the end of the first, 0-time task:
// Win1: [0-1000]. EQT of step [0-1000]: 500/2*1/2 = 125. EQT(win1) = 125/5 =
// 25.
// Win2: [0-1000],[2000-3000]. EQT of [2000-3000]: (1000+200)/2*4/5 = 480.
// EQT(win2) = (125+480)/5 = 121.
// Win3: [0-1000],[2000-3000],[11000-12000]. EQT of [11000-12000]: 0. EQT(win3)
// = 121.
// Win4: [0-1000],[2000-3000],[11000-13000]. EQT of [12000-13000]:
// (1500+1400)/2*1/10 = 145. EQT(win4) = (125+480+0+145)/5 = 150.
// Win5: [0-1000],[2000-3000],[11000-14000]. EQT of [13000-14000]: (1400+400)/2
// = 900. EQT(win5) = (125+480+0+145+900)/5 = 330.
// Win6: [2000-3000],[11000-15000]. EQT of [14000-15000]: 400/2*2/5 = 80.
// EQT(win6) = (480+0+145+900+80)/5 = 321.
// Win7: [11000-16000]. EQT of [15000-16000]: (2500+1700)/2*4/5 = 1680.
// EQT(win7) = (0+145+900+80+1680)/5 = 561.
// Win8: [12000-17000]. EQT of [16000-17000]: (1700+700)/2 = 1200. EQT(win8) =
// (145+900+80+1680+1200)/5 = 801.
TEST_F(QueueingTimeEstimatorTest, BackgroundedEQTsWithMutipleStepsPerWindow) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnRendererStateChanged(true, time);
  // This task should be ignored.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(400);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(2000);
  // These tasks should be ignored.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(3400);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  // Window with last step should not be reported.
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(25),
                                     base::TimeDelta::FromMilliseconds(121),
                                     base::TimeDelta::FromMilliseconds(121),
                                     base::TimeDelta::FromMilliseconds(150),
                                     base::TimeDelta::FromMilliseconds(330),
                                     base::TimeDelta::FromMilliseconds(321),
                                     base::TimeDelta::FromMilliseconds(561),
                                     base::TimeDelta::FromMilliseconds(801)));
}

// Split ExpectedQueueingTime only reports once per disjoint window. The
// following is a detailed explanation of EQT per window and task queue:
// Window 1: A 3000ms default queue task contributes 900 to that EQT.
// Window 2: Two 2000ms default loading queue tasks: 400 each, total 800 EQT.
// Window 3: 3000 ms default loading queue task: 900 EQT for that type. Also,
// the first 2000ms from a 3000ms default task: 800 EQT for that.
// Window 4: The remaining 100 EQT for default type. Also 1000ms tasks (which
// contribute 100) for FrameLoading, FrameThrottleable, and Unthrottled.
// Window 5: 600 ms tasks (which contribute 36) for each of the buckets except
// other. Two 300 ms (each contributing 9) and one 200 ms tasks (contributes 4)
// for the other bucket.
TEST_F(QueueingTimeEstimatorTest, SplitEQT) {
  HistogramTester histogram_tester;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  // Dummy task to initialize the estimator.
  estimator.OnTopLevelTaskStarted(time, kDefaultQueueType);
  estimator.OnTopLevelTaskCompleted(time);

  // Beginning of window 1.
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskStarted(time, QueueType::DEFAULT);
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1500);
  // Beginning of window 2.
  estimator.OnTopLevelTaskStarted(time, QueueType::DEFAULT_LOADING);

  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, QueueType::DEFAULT_LOADING);
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  // Beginning of window 3.
  estimator.OnTopLevelTaskStarted(time, QueueType::DEFAULT_LOADING);
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, QueueType::DEFAULT);
  time += base::TimeDelta::FromMilliseconds(3000);
  // 1000 ms after beginning of window 4.
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  const QueueType types_for_thousand[3] = {QueueType::FRAME_LOADING,
                                           QueueType::FRAME_THROTTLEABLE,
                                           QueueType::UNTHROTTLED};
  for (QueueType queue_type : types_for_thousand) {
    estimator.OnTopLevelTaskStarted(time, queue_type);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Beginning of window 5.
  const QueueType types_for_six_hundred[7] = {
      QueueType::DEFAULT,        QueueType::DEFAULT_LOADING,
      QueueType::FRAME_LOADING,  QueueType::FRAME_THROTTLEABLE,
      QueueType::FRAME_PAUSABLE, QueueType::UNTHROTTLED,
      QueueType::COMPOSITOR};
  for (QueueType queue_type : types_for_six_hundred) {
    estimator.OnTopLevelTaskStarted(time, queue_type);
    time += base::TimeDelta::FromMilliseconds(600);
    estimator.OnTopLevelTaskCompleted(time);
  }

  estimator.OnTopLevelTaskStarted(time, QueueType::OTHER);
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, QueueType::OTHER);
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, QueueType::OTHER);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnTopLevelTaskCompleted(time);

  // End of window 5. Now check the vectors per task queue type.
  EXPECT_THAT(client.GetValueFor(QueueType::DEFAULT),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 0, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 36, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 100, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 800, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default", 900, 1);

  EXPECT_THAT(client.GetValueFor(QueueType::DEFAULT_LOADING),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading",
      5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading", 0,
      2);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading",
      36, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading",
      800, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading",
      900, 1);

  EXPECT_THAT(client.GetValueFor(QueueType::FRAME_LOADING),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameLoading", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameLoading", 0,
      3);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameLoading", 36,
      1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameLoading", 100,
      1);

  EXPECT_THAT(client.GetValueFor(QueueType::FRAME_THROTTLEABLE),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameThrottleable",
      5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameThrottleable",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameThrottleable",
      36, 1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameThrottleable",
      100, 1);

  EXPECT_THAT(client.GetValueFor(QueueType::FRAME_PAUSABLE),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FramePausable", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FramePausable", 0,
      4);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FramePausable", 36,
      1);

  EXPECT_THAT(client.GetValueFor(QueueType::UNTHROTTLED),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Unthrottled", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Unthrottled", 0,
      3);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Unthrottled", 36,
      1);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Unthrottled", 100,
      1);

  EXPECT_THAT(client.GetValueFor(QueueType::COMPOSITOR),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Compositor", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Compositor", 0, 4);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Compositor", 36,
      1);

  EXPECT_THAT(client.GetValueFor(QueueType::OTHER),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(22)));
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Other", 5);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Other", 0, 4);
  histogram_tester.ExpectBucketCount(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Other", 22, 1);

  // Check that the sum of split EQT equals the total EQT for each window.
  base::TimeDelta expected_sums[] = {base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(1700),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(274)};
  for (size_t window = 1; window < 6; ++window) {
    base::TimeDelta sum;
    // Add up the reported split EQTs for that window.
    for (auto entry : client.split_eqts())
      sum += entry.second[window - 1];
    // Compare the split sum and the reported EQT for the disjoint window.
    EXPECT_EQ(expected_sums[window - 1], sum);
    EXPECT_EQ(expected_sums[window - 1],
              client.expected_queueing_times()[5 * window - 1]);
  }
}

}  // namespace scheduler
}  // namespace blink
