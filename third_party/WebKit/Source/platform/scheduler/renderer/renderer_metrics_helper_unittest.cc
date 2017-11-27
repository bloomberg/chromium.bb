// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_metrics_helper.h"

#include <memory>
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "platform/scheduler/test/fake_web_frame_scheduler.h"
#include "platform/scheduler/test/fake_web_view_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

using QueueType = MainThreadTaskQueue::QueueType;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using base::Bucket;

class RendererMetricsHelperTest : public ::testing::Test {
 public:
  RendererMetricsHelperTest() {}
  ~RendererMetricsHelperTest() {}

  void SetUp() {
    histogram_tester_.reset(new base::HistogramTester());
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(clock_.get(), true);
    scheduler_ = std::make_unique<RendererSchedulerImpl>(
        CreateTaskQueueManagerWithUnownedClockForTest(
            nullptr, mock_task_runner_, clock_.get()));
    metrics_helper_ = &scheduler_->main_thread_only().metrics_helper;
  }

  void TearDown() {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void RunTask(MainThreadTaskQueue::QueueType queue_type,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(clock_->NowTicks(), start);
    clock_->SetNowTicks(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(queue_type));

    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::Closure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration);
  }

  void RunTask(WebFrameScheduler* scheduler,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(clock_->NowTicks(), start);
    clock_->SetNowTicks(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(QueueType::kDefault));
    queue->SetFrameScheduler(scheduler);
    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::Closure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration);
  }

  base::TimeTicks Milliseconds(int milliseconds) {
    return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  void ForceUpdatePolicy() { scheduler_->ForceUpdatePolicy(); }

  std::unique_ptr<FakeWebFrameScheduler> CreateFakeWebFrameSchedulerWithType(
      FrameType frame_type) {
    FakeWebFrameScheduler::Builder builder;
    switch (frame_type) {
      case FrameType::kNone:
      case FrameType::kDetached:
        return nullptr;
      case FrameType::kMainFrameVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameType::kMainFrameVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameType::kMainFrameHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true);
        break;
      case FrameType::kMainFrameHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameType::kMainFrameBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame);
        break;
      case FrameType::kMainFrameBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameType::kMainFrameBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameType::kSameOriginVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameType::kSameOriginVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameType::kSameOriginHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true);
        break;
      case FrameType::kSameOriginHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameType::kSameOriginBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe);
        break;
      case FrameType::kSameOriginBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameType::kSameOriginBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameType::kCrossOriginVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameType::kCrossOriginVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameType::kCrossOriginHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true);
        break;
      case FrameType::kCrossOriginHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameType::kCrossOriginBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true);
        break;
      case FrameType::kCrossOriginBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameType::kCrossOriginBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameType::kCount:
        NOTREACHED();
        return nullptr;
    }
    return builder.Build();
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  RendererMetricsHelper* metrics_helper_;  // NOT OWNED
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FakeWebViewScheduler> playing_view_ =
      FakeWebViewScheduler::Builder().SetIsPlayingAudio(true).Build();
  std::unique_ptr<FakeWebViewScheduler> throtting_exempt_view_ =
      FakeWebViewScheduler::Builder().SetIsThrottlingExempt(true).Build();

  DISALLOW_COPY_AND_ASSIGN(RendererMetricsHelperTest);
};

TEST_F(RendererMetricsHelperTest, Metrics) {
  // QueueType::kDefault is checking sub-millisecond task aggregation,
  // FRAME_* tasks are checking normal task aggregation and other
  // queue types have a single task.
  RunTask(QueueType::kDefault, Milliseconds(1),
          base::TimeDelta::FromMicroseconds(700));
  RunTask(QueueType::kDefault, Milliseconds(2),
          base::TimeDelta::FromMicroseconds(700));
  RunTask(QueueType::kDefault, Milliseconds(3),
          base::TimeDelta::FromMicroseconds(700));

  RunTask(QueueType::kDefaultLoading, Milliseconds(200),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::kControl, Milliseconds(400),
          base::TimeDelta::FromMilliseconds(30));
  RunTask(QueueType::kDefaultTimer, Milliseconds(600),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::kFrameLoading, Milliseconds(800),
          base::TimeDelta::FromMilliseconds(70));
  RunTask(QueueType::kFramePausable, Milliseconds(1000),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::kCompositor, Milliseconds(1200),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::kTest, Milliseconds(1600),
          base::TimeDelta::FromMilliseconds(85));

  scheduler_->SetRendererBackgrounded(true);

  RunTask(QueueType::kControl, Milliseconds(2000),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::kFrameThrottleable, Milliseconds(2600),
          base::TimeDelta::FromMilliseconds(175));
  RunTask(QueueType::kUnthrottled, Milliseconds(2800),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::kFrameLoading, Milliseconds(3000),
          base::TimeDelta::FromMilliseconds(35));
  RunTask(QueueType::kFrameThrottleable, Milliseconds(3200),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::kCompositor, Milliseconds(3400),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::kIdle, Milliseconds(3600),
          base::TimeDelta::FromMilliseconds(50));
  RunTask(QueueType::kFrameLoading_kControl, Milliseconds(4000),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::kControl, Milliseconds(4200),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::kFrameThrottleable, Milliseconds(4400),
          base::TimeDelta::FromMilliseconds(115));
  RunTask(QueueType::kFramePausable, Milliseconds(4600),
          base::TimeDelta::FromMilliseconds(175));
  RunTask(QueueType::kIdle, Milliseconds(5000),
          base::TimeDelta::FromMilliseconds(1600));

  std::vector<base::Bucket> expected_samples = {
      {static_cast<int>(QueueType::kControl), 75},
      {static_cast<int>(QueueType::kDefault), 2},
      {static_cast<int>(QueueType::kDefaultLoading), 20},
      {static_cast<int>(QueueType::kDefaultTimer), 5},
      {static_cast<int>(QueueType::kUnthrottled), 25},
      {static_cast<int>(QueueType::kFrameLoading), 105},
      {static_cast<int>(QueueType::kCompositor), 45},
      {static_cast<int>(QueueType::kIdle), 1650},
      {static_cast<int>(QueueType::kTest), 85},
      {static_cast<int>(QueueType::kFrameLoading_kControl), 5},
      {static_cast<int>(QueueType::kFrameThrottleable), 295},
      {static_cast<int>(QueueType::kFramePausable), 195}};
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType2"),
              testing::ContainerEq(expected_samples));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType2.Foreground"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(QueueType::kControl), 30),
                  Bucket(static_cast<int>(QueueType::kDefault), 2),
                  Bucket(static_cast<int>(QueueType::kDefaultLoading), 20),
                  Bucket(static_cast<int>(QueueType::kDefaultTimer), 5),
                  Bucket(static_cast<int>(QueueType::kFrameLoading), 70),
                  Bucket(static_cast<int>(QueueType::kCompositor), 25),
                  Bucket(static_cast<int>(QueueType::kTest), 85),
                  Bucket(static_cast<int>(QueueType::kFramePausable), 20)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskDurationPerQueueType2.Background"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(QueueType::kControl), 45),
          Bucket(static_cast<int>(QueueType::kUnthrottled), 25),
          Bucket(static_cast<int>(QueueType::kFrameLoading), 35),
          Bucket(static_cast<int>(QueueType::kFrameThrottleable), 295),
          Bucket(static_cast<int>(QueueType::kFramePausable), 175),
          Bucket(static_cast<int>(QueueType::kCompositor), 20),
          Bucket(static_cast<int>(QueueType::kIdle), 1650),
          Bucket(static_cast<int>(QueueType::kFrameLoading_kControl), 5)));
}

TEST_F(RendererMetricsHelperTest, GetFrameTypeTest) {
  DCHECK_EQ(GetFrameType(nullptr), FrameType::kNone);

  FrameType frame_types_tested[] = {FrameType::kMainFrameVisible,
                                    FrameType::kSameOriginHidden,
                                    FrameType::kCrossOriginHidden,
                                    FrameType::kSameOriginBackground,
                                    FrameType::kMainFrameBackgroundExemptSelf,
                                    FrameType::kSameOriginVisibleService,
                                    FrameType::kCrossOriginHiddenService,
                                    FrameType::kMainFrameBackgroundExemptOther};
  for (FrameType frame_type : frame_types_tested) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(frame_type);
    EXPECT_EQ(GetFrameType(frame.get()), frame_type);
  }
}

TEST_F(RendererMetricsHelperTest, BackgroundedRendererTransition) {
  scheduler_->SetStoppingWhenBackgroundedEnabled(true);
  scheduler_->SetRendererBackgrounded(true);
  typedef BackgroundedRendererTransition Transition;

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded), 1)));

  scheduler_->SetRendererBackgrounded(false);
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded), 1),
                  Bucket(static_cast<int>(Transition::kForegrounded), 1)));

  scheduler_->SetRendererBackgrounded(true);
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded), 2),
                  Bucket(static_cast<int>(Transition::kForegrounded), 1)));

  // Waste 5+ minutes so that the delayed stop is triggered
  RunTask(QueueType::kDefault, Milliseconds(1),
          base::TimeDelta::FromSeconds(5 * 61));
  // Firing ForceUpdatePolicy multiple times to make sure that the metric is
  // only recorded upon an actual change.
  ForceUpdatePolicy();
  ForceUpdatePolicy();
  ForceUpdatePolicy();
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded), 2),
                  Bucket(static_cast<int>(Transition::kForegrounded), 1),
                  Bucket(static_cast<int>(Transition::kStoppedAfterDelay), 1)));

  scheduler_->SetRendererBackgrounded(false);
  ForceUpdatePolicy();
  ForceUpdatePolicy();
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded), 2),
                  Bucket(static_cast<int>(Transition::kForegrounded), 2),
                  Bucket(static_cast<int>(Transition::kStoppedAfterDelay), 1),
                  Bucket(static_cast<int>(Transition::kResumed), 1)));
}

TEST_F(RendererMetricsHelperTest, TaskCountPerFrameType) {
  int task_count = 0;
  struct CountPerFrameType {
    FrameType frame_type;
    int count;
  };
  CountPerFrameType test_data[] = {
      {FrameType::kNone, 4},
      {FrameType::kMainFrameVisible, 8},
      {FrameType::kMainFrameBackgroundExemptSelf, 5},
      {FrameType::kCrossOriginHidden, 3},
      {FrameType::kCrossOriginHiddenService, 7},
      {FrameType::kCrossOriginVisible, 1},
      {FrameType::kMainFrameBackgroundExemptOther, 2},
      {FrameType::kSameOriginVisible, 10},
      {FrameType::kSameOriginBackground, 9},
      {FrameType::kSameOriginVisibleService, 6}};

  for (const auto& data : test_data) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(data.frame_type);
    for (int i = 0; i < data.count; ++i) {
      RunTask(frame.get(), Milliseconds(++task_count),
              base::TimeDelta::FromMicroseconds(100));
    }
  }

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kNone), 4),
          Bucket(static_cast<int>(FrameType::kMainFrameVisible), 8),
          Bucket(static_cast<int>(FrameType::kMainFrameBackgroundExemptSelf),
                 5),
          Bucket(static_cast<int>(FrameType::kMainFrameBackgroundExemptOther),
                 2),
          Bucket(static_cast<int>(FrameType::kSameOriginVisible), 10),
          Bucket(static_cast<int>(FrameType::kSameOriginVisibleService), 6),
          Bucket(static_cast<int>(FrameType::kSameOriginBackground), 9),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisible), 1),
          Bucket(static_cast<int>(FrameType::kCrossOriginHidden), 3),
          Bucket(static_cast<int>(FrameType::kCrossOriginHiddenService), 7)));
}

TEST_F(RendererMetricsHelperTest, TaskCountPerFrameTypeLongerThan) {
  int total_duration = 0;
  struct TasksPerFrameType {
    FrameType frame_type;
    std::vector<int> durations;
  };
  TasksPerFrameType test_data[] = {
      {FrameType::kSameOriginHidden,
       {2, 15, 16, 20, 25, 30, 49, 50, 73, 99, 100, 110, 140, 150, 800, 1000,
        1200}},
      {FrameType::kCrossOriginVisibleService, {5, 10, 18, 19, 20, 55, 75, 220}},
      {FrameType::kMainFrameBackground,
       {21, 31, 41, 51, 61, 71, 81, 91, 101, 1001}},
  };

  for (const auto& data : test_data) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(data.frame_type);
    for (size_t i = 0; i < data.durations.size(); ++i) {
      RunTask(frame.get(), Milliseconds(++total_duration),
              base::TimeDelta::FromMilliseconds(data.durations[i]));
      total_duration += data.durations[i];
    }
  }

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameType::kSameOriginHidden), 17),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisibleService), 8)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan16ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameType::kSameOriginHidden), 15),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisibleService), 6)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan50ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kMainFrameBackground), 7),
          Bucket(static_cast<int>(FrameType::kSameOriginHidden), 10),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisibleService), 3)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan100ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kMainFrameBackground), 2),
          Bucket(static_cast<int>(FrameType::kSameOriginHidden), 7),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisibleService), 1)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan150ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameType::kMainFrameBackground), 1),
          Bucket(static_cast<int>(FrameType::kSameOriginHidden), 4),
          Bucket(static_cast<int>(FrameType::kCrossOriginVisibleService), 1)));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskCountPerFrameType.LongerThan1s"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(FrameType::kMainFrameBackground), 1),
                  Bucket(static_cast<int>(FrameType::kSameOriginHidden), 2)));
}

// TODO(crbug.com/754656): Add tests for NthMinute and AfterNthMinute
// histograms.

// TODO(crbug.com/754656): Add tests for TaskDuration.Hidden/Visible histograms.

// TODO(crbug.com/754656): Add tests for non-TaskDuration histograms.

}  // namespace scheduler
}  // namespace blink
