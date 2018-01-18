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
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "platform/scheduler/test/fake_web_frame_scheduler.h"
#include "platform/scheduler/test/fake_web_view_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/page/launching_process_state.h"

namespace blink {
namespace scheduler {

using QueueType = MainThreadTaskQueue::QueueType;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using base::Bucket;

class RendererMetricsHelperTest : public ::testing::Test {
 public:
  RendererMetricsHelperTest() = default;
  ~RendererMetricsHelperTest() = default;

  void SetUp() {
    histogram_tester_.reset(new base::HistogramTester());
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(&clock_, true);
    scheduler_ = std::make_unique<RendererSchedulerImpl>(
        CreateTaskQueueManagerForTest(nullptr, mock_task_runner_, &clock_));
    metrics_helper_ = &scheduler_->main_thread_only().metrics_helper;
  }

  void TearDown() {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void RunTask(MainThreadTaskQueue::QueueType queue_type,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(clock_.NowTicks(), start);
    clock_.SetNowTicks(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(queue_type));

    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::Closure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration, base::nullopt);
  }

  void RunTask(WebFrameScheduler* scheduler,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(clock_.NowTicks(), start);
    clock_.SetNowTicks(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(QueueType::kDefault));
    queue->SetFrameScheduler(scheduler);
    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::Closure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration, base::nullopt);
  }

  base::TimeTicks Milliseconds(int milliseconds) {
    return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  void ForceUpdatePolicy() { scheduler_->ForceUpdatePolicy(); }

  std::unique_ptr<FakeWebFrameScheduler> CreateFakeWebFrameSchedulerWithType(
      FrameStatus frame_status) {
    FakeWebFrameScheduler::Builder builder;
    switch (frame_status) {
      case FrameStatus::kNone:
      case FrameStatus::kDetached:
        return nullptr;
      case FrameStatus::kMainFrameVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kMainFrameHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameStatus::kMainFrameBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame);
        break;
      case FrameStatus::kMainFrameBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kMainFrameBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kSameOriginVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kSameOriginHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameStatus::kSameOriginBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe);
        break;
      case FrameStatus::kSameOriginBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kSameOriginBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCrossOriginVisible:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginVisibleService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginHidden:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kCrossOriginHiddenService:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(playing_view_.get());
        break;
      case FrameStatus::kCrossOriginBackground:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptSelf:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptOther:
        builder.SetFrameType(WebFrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetWebViewScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCount:
        NOTREACHED();
        return nullptr;
    }
    return builder.Build();
  }

  base::SimpleTestTickClock clock_;
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

  // Make sure that it starts in a foregrounded state.
  if (kLaunchingProcessIsBackgrounded)
    scheduler_->SetRendererBackgrounded(false);

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

TEST_F(RendererMetricsHelperTest, GetFrameStatusTest) {
  DCHECK_EQ(GetFrameStatus(nullptr), FrameStatus::kNone);

  FrameStatus frame_statuses_tested[] = {
      FrameStatus::kMainFrameVisible,
      FrameStatus::kSameOriginHidden,
      FrameStatus::kCrossOriginHidden,
      FrameStatus::kSameOriginBackground,
      FrameStatus::kMainFrameBackgroundExemptSelf,
      FrameStatus::kSameOriginVisibleService,
      FrameStatus::kCrossOriginHiddenService,
      FrameStatus::kMainFrameBackgroundExemptOther};
  for (FrameStatus frame_status : frame_statuses_tested) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(frame_status);
    EXPECT_EQ(GetFrameStatus(frame.get()), frame_status);
  }
}

TEST_F(RendererMetricsHelperTest, BackgroundedRendererTransition) {
  scheduler_->SetStoppingWhenBackgroundedEnabled(true);
  typedef BackgroundedRendererTransition Transition;

  int backgrounding_transitions = 0;
  int foregrounding_transitions = 0;
  if (!kLaunchingProcessIsBackgrounded) {
    scheduler_->SetRendererBackgrounded(true);
    backgrounding_transitions++;
    EXPECT_THAT(
        histogram_tester_->GetAllSamples(
            "RendererScheduler.BackgroundedRendererTransition"),
        UnorderedElementsAre(Bucket(static_cast<int>(Transition::kBackgrounded),
                                    backgrounding_transitions)));
    scheduler_->SetRendererBackgrounded(false);
    foregrounding_transitions++;
    EXPECT_THAT(
        histogram_tester_->GetAllSamples(
            "RendererScheduler.BackgroundedRendererTransition"),
        UnorderedElementsAre(Bucket(static_cast<int>(Transition::kBackgrounded),
                                    backgrounding_transitions),
                             Bucket(static_cast<int>(Transition::kForegrounded),
                                    foregrounding_transitions)));
  } else {
    scheduler_->SetRendererBackgrounded(false);
    foregrounding_transitions++;
    EXPECT_THAT(
        histogram_tester_->GetAllSamples(
            "RendererScheduler.BackgroundedRendererTransition"),
        UnorderedElementsAre(Bucket(static_cast<int>(Transition::kForegrounded),
                                    foregrounding_transitions)));
  }

  scheduler_->SetRendererBackgrounded(true);
  backgrounding_transitions++;
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.BackgroundedRendererTransition"),
      UnorderedElementsAre(Bucket(static_cast<int>(Transition::kBackgrounded),
                                  backgrounding_transitions),
                           Bucket(static_cast<int>(Transition::kForegrounded),
                                  foregrounding_transitions)));

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
                  Bucket(static_cast<int>(Transition::kBackgrounded),
                         backgrounding_transitions),
                  Bucket(static_cast<int>(Transition::kForegrounded),
                         foregrounding_transitions),
                  Bucket(static_cast<int>(Transition::kStoppedAfterDelay), 1)));

  scheduler_->SetRendererBackgrounded(false);
  foregrounding_transitions++;
  ForceUpdatePolicy();
  ForceUpdatePolicy();
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.BackgroundedRendererTransition"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(Transition::kBackgrounded),
                         backgrounding_transitions),
                  Bucket(static_cast<int>(Transition::kForegrounded),
                         foregrounding_transitions),
                  Bucket(static_cast<int>(Transition::kStoppedAfterDelay), 1),
                  Bucket(static_cast<int>(Transition::kResumed), 1)));
}

TEST_F(RendererMetricsHelperTest, TaskCountPerFrameStatus) {
  int task_count = 0;
  struct CountPerFrameStatus {
    FrameStatus frame_status;
    int count;
  };
  CountPerFrameStatus test_data[] = {
      {FrameStatus::kNone, 4},
      {FrameStatus::kMainFrameVisible, 8},
      {FrameStatus::kMainFrameBackgroundExemptSelf, 5},
      {FrameStatus::kCrossOriginHidden, 3},
      {FrameStatus::kCrossOriginHiddenService, 7},
      {FrameStatus::kCrossOriginVisible, 1},
      {FrameStatus::kMainFrameBackgroundExemptOther, 2},
      {FrameStatus::kSameOriginVisible, 10},
      {FrameStatus::kSameOriginBackground, 9},
      {FrameStatus::kSameOriginVisibleService, 6}};

  for (const auto& data : test_data) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(data.frame_status);
    for (int i = 0; i < data.count; ++i) {
      RunTask(frame.get(), Milliseconds(++task_count),
              base::TimeDelta::FromMicroseconds(100));
    }
  }

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kNone), 4),
          Bucket(static_cast<int>(FrameStatus::kMainFrameVisible), 8),
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackgroundExemptSelf),
                 5),
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackgroundExemptOther),
                 2),
          Bucket(static_cast<int>(FrameStatus::kSameOriginVisible), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginVisibleService), 6),
          Bucket(static_cast<int>(FrameStatus::kSameOriginBackground), 9),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisible), 1),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginHidden), 3),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginHiddenService), 7)));
}

TEST_F(RendererMetricsHelperTest, TaskCountPerFrameTypeLongerThan) {
  int total_duration = 0;
  struct TasksPerFrameStatus {
    FrameStatus frame_status;
    std::vector<int> durations;
  };
  TasksPerFrameStatus test_data[] = {
      {FrameStatus::kSameOriginHidden,
       {2, 15, 16, 20, 25, 30, 49, 50, 73, 99, 100, 110, 140, 150, 800, 1000,
        1200}},
      {FrameStatus::kCrossOriginVisibleService,
       {5, 10, 18, 19, 20, 55, 75, 220}},
      {FrameStatus::kMainFrameBackground,
       {21, 31, 41, 51, 61, 71, 81, 91, 101, 1001}},
  };

  for (const auto& data : test_data) {
    std::unique_ptr<FakeWebFrameScheduler> frame =
        CreateFakeWebFrameSchedulerWithType(data.frame_status);
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
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 17),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 8)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan16ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 15),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 6)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan50ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 7),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 10),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 3)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan100ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 2),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 7),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 1)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan150ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 1),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 4),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 1)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan1s"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 1),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 2)));
}

// TODO(crbug.com/754656): Add tests for NthMinute and AfterNthMinute
// histograms.

// TODO(crbug.com/754656): Add tests for TaskDuration.Hidden/Visible histograms.

// TODO(crbug.com/754656): Add tests for non-TaskDuration histograms.

}  // namespace scheduler
}  // namespace blink
