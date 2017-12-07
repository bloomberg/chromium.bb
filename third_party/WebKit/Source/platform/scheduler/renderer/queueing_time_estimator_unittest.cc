// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/queueing_time_estimator.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/fake_web_frame_scheduler.h"
#include "platform/testing/HistogramTester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <map>
#include <string>
#include <vector>

namespace blink {
namespace scheduler {

using QueueType = MainThreadTaskQueue::QueueType;

namespace {

class TestQueueingTimeEstimatorClient : public QueueingTimeEstimator::Client {
 public:
  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override {
    expected_queueing_times_.push_back(queueing_time);
    // Mimic RendererSchedulerImpl::OnQueueingTimeForWindowEstimated.
    if (is_disjoint_window) {
      UMA_HISTOGRAM_TIMES("RendererScheduler.ExpectedTaskQueueingDuration",
                          queueing_time);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "RendererScheduler.ExpectedTaskQueueingDuration2",
          queueing_time.InMicroseconds(),
          RendererSchedulerImpl::kMinExpectedQueueingTimeBucket,
          RendererSchedulerImpl::kMaxExpectedQueueingTimeBucket,
          RendererSchedulerImpl::kNumberExpectedQueueingTimeBuckets);
    }
  }
  void OnReportSplitExpectedQueueingTime(
      const char* split_description,
      base::TimeDelta queueing_time) override {
    if (split_eqts_.find(split_description) == split_eqts_.end())
      split_eqts_[split_description] = std::vector<base::TimeDelta>();
    split_eqts_[split_description].push_back(queueing_time);
    // Mimic RendererSchedulerImpl::OnReportSplitExpectedQueueingTime.
    base::UmaHistogramTimes(split_description, queueing_time);
  }
  void OnReportFineGrainedExpectedQueueingTime(const char* split_description,
                                               base::TimeDelta queueing_time) {
    // Mimic RendererSchedulerImpl::OnReportFineGrainedExpectedQueueingTime.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        split_description, queueing_time.InMicroseconds(),
        RendererSchedulerImpl::kMinExpectedQueueingTimeBucket,
        RendererSchedulerImpl::kMaxExpectedQueueingTimeBucket,
        RendererSchedulerImpl::kNumberExpectedQueueingTimeBuckets);
  }
  const std::vector<base::TimeDelta>& expected_queueing_times() {
    return expected_queueing_times_;
  }
  const std::map<const char*, std::vector<base::TimeDelta>>& split_eqts() {
    return split_eqts_;
  }
  const std::vector<base::TimeDelta>& QueueTypeValues(QueueType queue_type) {
    return split_eqts_[QueueingTimeEstimator::Calculator::
                           GetReportingMessageFromQueueType(queue_type)];
  }
  const std::vector<base::TimeDelta>& FrameStatusValues(
      FrameStatus frame_status) {
    return split_eqts_[QueueingTimeEstimator::Calculator::
                           GetReportingMessageFromFrameStatus(frame_status)];
  }

 private:
  std::vector<base::TimeDelta> expected_queueing_times_;
  std::map<const char*, std::vector<base::TimeDelta>> split_eqts_;
};

class QueueingTimeEstimatorForTest : public QueueingTimeEstimator {
 public:
  QueueingTimeEstimatorForTest(TestQueueingTimeEstimatorClient* client,
                               base::TimeDelta window_duration,
                               int steps_per_window)
      : QueueingTimeEstimator(client, window_duration, steps_per_window) {}
};

struct BucketExpectation {
  int sample;
  int count;
};

}  // namespace

class QueueingTimeEstimatorTest : public ::testing::Test {
 protected:
  static std::vector<BucketExpectation> GetFineGrained(
      const std::vector<BucketExpectation>& expected) {
    std::vector<BucketExpectation> fine_grained(expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      fine_grained[i].sample = expected[i].sample * 1000;
      fine_grained[i].count = expected[i].count;
    }
    return fine_grained;
  }

  void TestHistogram(const std::string& name,
                     int total,
                     const std::vector<BucketExpectation>& expectations) {
    histogram_tester.ExpectTotalCount(name, total);
    int sum = 0;
    for (const auto& expected : expectations) {
      histogram_tester.ExpectBucketCount(name, expected.sample, expected.count);
      sum += expected.count;
    }
    EXPECT_EQ(total, sum);
  }

  void TestSplitSumsTotal(base::TimeDelta* expected_sums, int num_windows) {
    for (int window = 1; window < num_windows; ++window) {
      base::TimeDelta sum;
      // Add up the reported split EQTs for that window.
      for (const auto& entry : client.split_eqts())
        sum += entry.second[window - 1];
      // Divide sum by two because we're also adding the split by frame type.
      sum /= 2.0;
      // Compare the split sum and the reported EQT for the disjoint window.
      EXPECT_EQ(expected_sums[window - 1], sum);
      EXPECT_EQ(expected_sums[window - 1],
                client.expected_queueing_times()[5 * window - 1]);
    }
  }

  HistogramTester histogram_tester;
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
};

// Three tasks of one second each, all within a 5 second window. Expected
// queueing time is the probability of falling into one of these tasks (3/5),
// multiplied by the expected queueing time within a task (0.5 seconds). Thus we
// expect a queueing time of 0.3 seconds.
TEST_F(QueueingTimeEstimatorTest, AllTasksWithinWindow) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  for (int i = 0; i < 3; ++i) {
    estimator.OnTopLevelTaskStarted(time, nullptr);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
  std::vector<BucketExpectation> expected = {{300, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 1, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 1,
                fine_grained);
}

// One 20 second long task, starting 3 seconds into the first window.
// Window 1: Probability of being within task = 2/5. Expected delay within task:
// avg(20, 18). Total expected queueing time = 7.6s.
// Window 2: Probability of being within task = 1. Expected delay within task:
// avg(18, 13). Total expected queueing time = 15.5s.
// Window 5: Probability of being within task = 3/5. Expected delay within task:
// avg(3, 0). Total expected queueing time = 0.9s.
TEST_F(QueueingTimeEstimatorTest, MultiWindowTask) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                     base::TimeDelta::FromMilliseconds(15500),
                                     base::TimeDelta::FromMilliseconds(10500),
                                     base::TimeDelta::FromMilliseconds(5500),
                                     base::TimeDelta::FromMilliseconds(900)));
  std::vector<BucketExpectation> expected = {
      {900, 1}, {5500, 1}, {7600, 1}, {10500, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  // Split here is different: 4 values go into the highest bucket.
  std::vector<BucketExpectation> fine_grained = {{900 * 1000, 1},
                                                 {5500 * 1000, 4}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 5,
                fine_grained);
}

// The main thread is considered unresponsive during a single long task. In this
// case, the single long task is 3 seconds long.
// Probability of being with the task = 3/5. Expected delay within task:
// avg(0, 3). Total expected queueing time = 3/5 * 3/2 = 0.9s.
// In this example, the queueing time comes from the current, incomplete window.
TEST_F(QueueingTimeEstimatorTest,
       EstimateQueueingTimeDuringSingleLongTaskIncompleteWindow) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, nullptr);

  time += base::TimeDelta::FromMilliseconds(3000);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(900), estimated_queueing_time);
  // Window time was not completed, so no UMA should be recorded.
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0);
  histogram_tester.ExpectTotalCount("ExpectedTaskQueueingDuration2", 0);
}

// The main thread is considered unresponsive during a single long task, which
// exceeds the size of one window. We report the queueing time of the most
// recent window. Probability of being within the task = 100%, as the task
// fills the whole window. Expected delay within this task = avg(8, 3) = 5.5.
TEST_F(QueueingTimeEstimatorTest,
       EstimateQueueingTimeDuringSingleLongTaskExceedingWindow) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, nullptr);

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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, nullptr);

  time += base::TimeDelta::FromMilliseconds(5500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3000), estimated_queueing_time);
  // Only EstimateQueueingTimeIncludingCurrentTask has been called after window
  // completion, so UMA should not have been reported yet.
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration", 0);
  histogram_tester.ExpectTotalCount(
      "RendererScheduler.ExpectedTaskQueueingDuration2", 0);
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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(5000);
  base::TimeTicks start_time = time;
  estimator.OnTopLevelTaskStarted(start_time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);

  base::TimeDelta estimated_queueing_time =
      estimator.EstimateQueueingTimeIncludingCurrentTask(time);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(25), estimated_queueing_time);
  std::vector<BucketExpectation> expected = {{0, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 1, expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 1, expected);
}

// Tasks containing nested run loops may be extremely long without
// negatively impacting user experience. Ignore such tasks.
TEST_F(QueueingTimeEstimatorTest, IgnoresTasksWithNestedMessageLoops) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(5000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnBeginNestedRunLoop();
  estimator.OnTopLevelTaskCompleted(time);

  // Perform an additional task after the nested run loop. A 1 second task
  // in a 5 second window results in a 100ms expected queueing time.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100)));
  std::vector<BucketExpectation> expected = {{0, 1}, {100, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 2,
                fine_grained);
}

// If a task is too long, we assume it's invalid. Perhaps the user's machine
// went to sleep during a task, resulting in an extremely long task. Ignore
// these long tasks completely.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongTasks) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(4000);

  // Now perform an invalid task. This will cause the windows involving this
  // task to be ignored.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnTopLevelTaskCompleted(time);

  // Perform another 1 second task.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  // Now perform another invalid task. This will cause the windows involving
  // this task to be ignored. Therefore, the previous task is ignored.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnTopLevelTaskCompleted(time);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(100)));
  std::vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 2,
                fine_grained);
}

// If we idle for too long, ignore idling time, even if the renderer is on the
// foreground. Perhaps the user's machine went to sleep while we were idling.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongIdlePeriods) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 1);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(4000);
  // Dummy task to ensure this window is reported.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  // Now go idle for long. This will cause the windows involving this
  // time to be ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Perform another 1 second task.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  // Now go idle again. This will cause the windows involving this idle period
  // to be ignored. Therefore, the previous task is ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(100)));
  std::vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 2,
                fine_grained);
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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
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
  std::vector<BucketExpectation> expected = {{0, 1}, {2500, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(500);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
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
  std::vector<BucketExpectation> expected = {{0, 1}, {725, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 2,
                fine_grained);
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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(4000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnTopLevelTaskStarted(time, nullptr);
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
  std::vector<BucketExpectation> expected = {{325, 1}, {400, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 2,
                fine_grained);
}

// There are multiple windows, but some of the EQTs are not reported due to
// backgrounded renderer. EQT(win1) = 0. EQT(win3) = (1500+500)/2 = 1000.
// EQT(win4) = 1/2*500/2 = 250. EQT(win7) = 1/5*200/2 = 20.
TEST_F(QueueingTimeEstimatorTest, BackgroundedEQTsWithSingleStepPerWindow) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(1), 1);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(1001);

  // Second window should not be reported.
  estimator.OnRendererStateChanged(true, time);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(456);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRendererStateChanged(false, time);
  time += base::TimeDelta::FromMilliseconds(343);

  // Third, fourth windows should be reported
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnTopLevelTaskCompleted(time);
  time += base::TimeDelta::FromMilliseconds(501);

  // Fifth, sixth task should not be reported
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRendererStateChanged(false, time);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(999);

  // Seventh task should be reported.
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  EXPECT_THAT(client.expected_queueing_times(),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(1000),
                                     base::TimeDelta::FromMilliseconds(125),
                                     base::TimeDelta::FromMilliseconds(20)));
  std::vector<BucketExpectation> expected = {
      {0, 1}, {20, 1}, {125, 1}, {1000, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 4, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 4,
                fine_grained);
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
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnRendererStateChanged(true, time);
  // This task should be ignored.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(400);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(2000);
  // These tasks should be ignored.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(3400);
  estimator.OnTopLevelTaskCompleted(time);
  estimator.OnRendererStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnTopLevelTaskCompleted(time);

  // Window with last step should not be reported.
  estimator.OnRendererStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, nullptr);
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
TEST_F(QueueingTimeEstimatorTest, SplitEQTByTaskQueueType) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Dummy task to initialize the estimator.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);

  // Beginning of window 1.
  time += base::TimeDelta::FromMilliseconds(500);
  scoped_refptr<MainThreadTaskQueueForTest> default_queue(
      new MainThreadTaskQueueForTest(QueueType::kDefault));
  estimator.OnTopLevelTaskStarted(time, default_queue.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1500);
  // Beginning of window 2.
  scoped_refptr<MainThreadTaskQueueForTest> default_loading_queue(
      new MainThreadTaskQueueForTest(QueueType::kDefaultLoading));
  estimator.OnTopLevelTaskStarted(time, default_loading_queue.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, default_loading_queue.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  // Beginning of window 3.
  estimator.OnTopLevelTaskStarted(time, default_loading_queue.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, default_queue.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  // 1000 ms after beginning of window 4.
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  scoped_refptr<MainThreadTaskQueueForTest> frame_loading_queue(
      new MainThreadTaskQueueForTest(QueueType::kFrameLoading));
  scoped_refptr<MainThreadTaskQueueForTest> frame_throttleable_queue(
      new MainThreadTaskQueueForTest(QueueType::kFrameThrottleable));
  scoped_refptr<MainThreadTaskQueueForTest> unthrottled_queue(
      new MainThreadTaskQueueForTest(QueueType::kUnthrottled));
  MainThreadTaskQueue* queues_for_thousand[] = {frame_loading_queue.get(),
                                                frame_throttleable_queue.get(),
                                                unthrottled_queue.get()};
  for (auto queue : queues_for_thousand) {
    estimator.OnTopLevelTaskStarted(time, queue);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Beginning of window 5.
  scoped_refptr<MainThreadTaskQueueForTest> frame_pausable_queue(
      new MainThreadTaskQueueForTest(QueueType::kFramePausable));
  scoped_refptr<MainThreadTaskQueueForTest> compositor_queue(
      new MainThreadTaskQueueForTest(QueueType::kCompositor));
  MainThreadTaskQueue* queues_for_six_hundred[] = {
      default_queue.get(),        default_loading_queue.get(),
      frame_loading_queue.get(),  frame_throttleable_queue.get(),
      frame_pausable_queue.get(), unthrottled_queue.get(),
      compositor_queue.get()};
  for (auto queue : queues_for_six_hundred) {
    estimator.OnTopLevelTaskStarted(time, queue);
    time += base::TimeDelta::FromMilliseconds(600);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // The following task contributes to "Other" because kControl is not a
  // supported queue type.
  scoped_refptr<MainThreadTaskQueueForTest> control_queue(
      new MainThreadTaskQueueForTest(QueueType::kControl));
  estimator.OnTopLevelTaskStarted(time, control_queue.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  // The following task contributes to "Other" because kTest is not a supported
  // queue type.
  scoped_refptr<MainThreadTaskQueueForTest> test_queue(
      new MainThreadTaskQueueForTest(QueueType::kTest));
  estimator.OnTopLevelTaskStarted(time, test_queue.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  // The following task contributes to "Other" because there is no task queue.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnTopLevelTaskCompleted(time);

  // End of window 5. Now check the vectors per task queue type.
  EXPECT_THAT(client.QueueTypeValues(QueueType::kDefault),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  std::vector<BucketExpectation> expected = {
      {0, 1}, {36, 1}, {100, 1}, {800, 1}, {900, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Default",
                5, expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kDefaultLoading),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 2}, {36, 1}, {800, 1}, {900, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.DefaultLoading", 5,
      expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFrameLoading),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 3}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameLoading", 5,
      expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFrameThrottleable),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 3}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FrameThrottleable",
      5, expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFramePausable),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 4}, {36, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.FramePausable", 5,
      expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kUnthrottled),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 3}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Unthrottled", 5,
      expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kCompositor),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 4}, {36, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Compositor", 5,
      expected);

  EXPECT_THAT(client.QueueTypeValues(QueueType::kOther),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(22)));
  expected = {{0, 4}, {22, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Other",
                5, expected);

  // Check that the sum of split EQT equals the total EQT for each window.
  base::TimeDelta expected_sums[] = {base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(1700),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(274)};
  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kNone),
              ::testing::ElementsAreArray(expected_sums));
  expected = {{274, 1}, {400, 1}, {800, 1}, {900, 1}, {1700, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByFrameType.Other", 5,
                expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 5,
                fine_grained);
  TestSplitSumsTotal(expected_sums, 6);
}

// Split ExpectedQueueingTime only reports once per disjoint window. The
// following is a detailed explanation of EQT per window and frame type:
// Window 1: A 3000ms task in a background main frame contributes 900 to that
// EQT.
// Window 2: Two 2000ms tasks in a visible main frame: 400 each, total 800
// EQT.
// Window 3: 3000ms task in a visible main frame: 900 EQT for that type. Also,
// the first 2000ms from a 3000ms task in a background main frame: 800 EQT for
// that.
// Window 4: The remaining 100 EQT for background main frame. Also 1000ms
// tasks (which contribute 100) for kSameOriginVisible, kSameOriginHidden,
// and kCrossOriginVisible.
// Window 5: 400 ms tasks (which contribute 16) for each of the buckets except
// other. Two 300 ms (each contributing 9) and one 800 ms tasks (contributes
// 64) for the other bucket.
TEST_F(QueueingTimeEstimatorTest, SplitEQTByFrameStatus) {
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5), 5);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Dummy task to initialize the estimator.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  estimator.OnTopLevelTaskCompleted(time);
  scoped_refptr<MainThreadTaskQueueForTest> queue1(
      new MainThreadTaskQueueForTest(QueueType::kTest));

  // Beginning of window 1.
  time += base::TimeDelta::FromMilliseconds(500);
  // Scheduler with frame type: MAIN_FRAME_BACKGROUND.
  std::unique_ptr<FakeWebFrameScheduler> frame1 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .Build();
  queue1->SetFrameScheduler(frame1.get());
  estimator.OnTopLevelTaskStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1500);
  // Beginning of window 2.
  // Scheduler with frame type: MAIN_FRAME_VISIBLE.
  std::unique_ptr<FakeWebFrameScheduler> frame2 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .Build();
  queue1->SetFrameScheduler(frame2.get());
  estimator.OnTopLevelTaskStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  scoped_refptr<MainThreadTaskQueueForTest> queue2(
      new MainThreadTaskQueueForTest(QueueType::kTest));
  queue2->SetFrameScheduler(frame2.get());
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnTopLevelTaskStarted(time, queue2.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnTopLevelTaskCompleted(time);

  // Beginning of window 3.
  // Scheduler with frame type: MAIN_FRAME_VISIBLE.
  std::unique_ptr<FakeWebFrameScheduler> frame3 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .SetIsExemptFromThrottling(true)
          .Build();
  queue1->SetFrameScheduler(frame3.get());
  estimator.OnTopLevelTaskStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnTopLevelTaskCompleted(time);

  // Scheduler with frame type: MAIN_FRAME_BACKGROUND.
  std::unique_ptr<FakeWebFrameScheduler> frame4 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsFrameVisible(true)
          .SetIsExemptFromThrottling(true)
          .Build();
  queue1->SetFrameScheduler(frame4.get());
  estimator.OnTopLevelTaskStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  // 1000 ms after beginning of window 4.
  estimator.OnTopLevelTaskCompleted(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  // Scheduler with frame type: SAME_ORIGIN_VISIBLE.
  std::unique_ptr<FakeWebFrameScheduler> frame5 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .Build();
  // Scheduler with frame type: SAME_ORIGIN_HIDDEN.
  std::unique_ptr<FakeWebFrameScheduler> frame6 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_VISIBLE.
  std::unique_ptr<FakeWebFrameScheduler> frame7 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .SetIsCrossOrigin(true)
          .Build();
  FakeWebFrameScheduler* schedulers_for_thousand[] = {
      frame5.get(), frame6.get(), frame7.get()};
  for (auto scheduler : schedulers_for_thousand) {
    queue1->SetFrameScheduler(scheduler);
    estimator.OnTopLevelTaskStarted(time, queue1.get());
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // Beginning of window 5.
  // Scheduler with frame type: MAIN_FRAME_HIDDEN.
  std::unique_ptr<FakeWebFrameScheduler> frame8 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .Build();
  // Scheduler with frame type: SAME_ORIGIN_BACKGROUND.
  std::unique_ptr<FakeWebFrameScheduler> frame9 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_HIDDEN.
  std::unique_ptr<FakeWebFrameScheduler> frame10 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsCrossOrigin(true)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_BACKGROUND.
  std::unique_ptr<FakeWebFrameScheduler> frame11 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsCrossOrigin(true)
          .Build();
  // One scheduler per supported frame type, excluding "Other".
  FakeWebFrameScheduler* schedulers_for_four_hundred[] = {
      frame2.get(), frame1.get(), frame8.get(),  frame5.get(), frame6.get(),
      frame9.get(), frame7.get(), frame10.get(), frame11.get()};
  for (auto scheduler : schedulers_for_four_hundred) {
    queue1->SetFrameScheduler(scheduler);
    estimator.OnTopLevelTaskStarted(time, queue1.get());
    time += base::TimeDelta::FromMilliseconds(400);
    estimator.OnTopLevelTaskCompleted(time);
  }

  // The following tasks contribute to "Other" because there is no frame.
  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  queue1->SetFrameScheduler(nullptr);
  estimator.OnTopLevelTaskStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnTopLevelTaskCompleted(time);

  estimator.OnTopLevelTaskStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnTopLevelTaskCompleted(time);

  // End of window 5. Now check the vectors per frame type.
  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kMainFrameBackground),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(16)));
  std::vector<BucketExpectation> expected = {
      {0, 1}, {16, 1}, {100, 1}, {800, 1}, {900, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByFrameType.MainFrameBackground",
      5, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByFrameStatus.MainFrameBackground",
      5, fine_grained);

  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kMainFrameVisible),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(16)));
  expected = {{0, 2}, {16, 1}, {800, 1}, {900, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByFrameType.MainFrameVisible", 5,
      expected);

  struct FrameExpectation {
    FrameStatus frame_status;
    std::string name;
  };
  FrameExpectation three_expected[] = {
      {FrameStatus::kSameOriginVisible,
       "RendererScheduler.ExpectedQueueingTimeByFrameType.SameOriginVisible"},
      {FrameStatus::kSameOriginHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameType.SameOriginHidden"},
      {FrameStatus::kCrossOriginVisible,
       "RendererScheduler.ExpectedQueueingTimeByFrameType.CrossOriginVisible"},
  };
  for (const auto& frame_expectation : three_expected) {
    EXPECT_THAT(client.FrameStatusValues(frame_expectation.frame_status),
                ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(100),
                                       base::TimeDelta::FromMilliseconds(16)));
    expected = {{0, 3}, {16, 1}, {100, 1}};
    TestHistogram(frame_expectation.name, 5, expected);
  }

  FrameExpectation more_expected[] = {
      {FrameStatus::kMainFrameHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameType."
       "MainFrameHidden"},
      {FrameStatus::kSameOriginBackground,
       "RendererScheduler.ExpectedQueueingTimeByFrameType."
       "SameOriginBackground"},
      {FrameStatus::kCrossOriginHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameType.CrossOriginHidden"},
      {FrameStatus::kCrossOriginBackground,
       "RendererScheduler.ExpectedQueueingTimeByFrameType."
       "CrossOriginBackground"}};
  for (const auto& frame_expectation : more_expected) {
    EXPECT_THAT(client.FrameStatusValues(frame_expectation.frame_status),
                ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(0),
                                       base::TimeDelta::FromMilliseconds(16)));
    expected = {{0, 4}, {16, 1}};
    TestHistogram(frame_expectation.name, 5, expected);
  }

  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kNone),
              ::testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(82)));
  expected = {{0, 4}, {82, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByFrameType.Other", 5,
                expected);

  // Check that the sum of split EQT equals the total EQT for each window.
  base::TimeDelta expected_sums[] = {base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(1700),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(226)};
  EXPECT_THAT(client.QueueTypeValues(QueueType::kOther),
              ::testing::ElementsAreArray(expected_sums));
  expected = {{226, 1}, {400, 1}, {800, 1}, {900, 1}, {1700, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueueType.Other",
                5, expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration2", 5,
                fine_grained);
  TestSplitSumsTotal(expected_sums, 6);
}

}  // namespace scheduler
}  // namespace blink
