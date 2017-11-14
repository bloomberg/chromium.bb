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
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
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
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, std::make_unique<TestTimeSource>(clock_.get()));
    scheduler_ = std::make_unique<RendererSchedulerImpl>(delegate_);
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
    metrics_helper_->RecordTaskMetrics(queue.get(), start, start + duration);
  }

  base::TimeTicks Microseconds(int microseconds) {
    return base::TimeTicks() + base::TimeDelta::FromMicroseconds(microseconds);
  }

  base::TimeTicks Milliseconds(int milliseconds) {
    return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  void ForceUpdatePolicy() { scheduler_->ForceUpdatePolicy(); }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  RendererMetricsHelper* metrics_helper_;  // NOT OWNED
  std::unique_ptr<base::HistogramTester> histogram_tester_;

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

  std::unique_ptr<FakeWebFrameScheduler> frame1 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .Build();
  EXPECT_EQ(GetFrameType(frame1.get()), FrameType::kMainFrameVisible);

  std::unique_ptr<FakeWebFrameScheduler> frame2 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .Build();
  EXPECT_EQ(GetFrameType(frame2.get()), FrameType::kSameOriginHidden);

  std::unique_ptr<FakeWebFrameScheduler> frame3 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsCrossOrigin(true)
          .Build();
  EXPECT_EQ(GetFrameType(frame3.get()), FrameType::kCrossOriginHidden);

  std::unique_ptr<FakeWebFrameScheduler> frame4 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .Build();
  EXPECT_EQ(GetFrameType(frame4.get()), FrameType::kSameOriginBackground);

  std::unique_ptr<FakeWebFrameScheduler> frame5 =
      FakeWebFrameScheduler::Builder()
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .SetIsExemptFromThrottling(true)
          .Build();
  DCHECK_EQ(GetFrameType(frame5.get()),
            FrameType::kMainFrameBackgroundExemptSelf);

  std::unique_ptr<FakeWebViewScheduler> view1 =
      FakeWebViewScheduler::Builder().SetIsPlayingAudio(true).Build();

  std::unique_ptr<FakeWebFrameScheduler> frame6 =
      FakeWebFrameScheduler::Builder()
          .SetWebViewScheduler(view1.get())
          .SetIsFrameVisible(true)
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .Build();
  DCHECK_EQ(GetFrameType(frame6.get()), FrameType::kSameOriginVisibleService);

  std::unique_ptr<FakeWebFrameScheduler> frame7 =
      FakeWebFrameScheduler::Builder()
          .SetWebViewScheduler(view1.get())
          .SetIsCrossOrigin(true)
          .SetFrameType(WebFrameScheduler::FrameType::kSubframe)
          .Build();
  DCHECK_EQ(GetFrameType(frame7.get()), FrameType::kCrossOriginHiddenService);

  std::unique_ptr<FakeWebViewScheduler> view2 =
      FakeWebViewScheduler::Builder().SetIsThrottlingExempt(true).Build();

  std::unique_ptr<FakeWebFrameScheduler> frame8 =
      FakeWebFrameScheduler::Builder()
          .SetWebViewScheduler(view2.get())
          .SetFrameType(WebFrameScheduler::FrameType::kMainFrame)
          .Build();
  DCHECK_EQ(GetFrameType(frame8.get()),
            FrameType::kMainFrameBackgroundExemptOther);
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

// TODO(crbug.com/754656): Add tests for NthMinute and AfterNthMinute
// histograms.

// TODO(crbug.com/754656): Add tests for TaskDuration.Hidden/Visible histograms.

// TODO(crbug.com/754656): Add tests for non-TaskDuration histograms.

}  // namespace scheduler
}  // namespace blink
