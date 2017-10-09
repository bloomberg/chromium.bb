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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

using QueueType = MainThreadTaskQueue::QueueType;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using base::Bucket;

namespace {

class MainThreadTaskQueueForTest : public MainThreadTaskQueue {
 public:
  MainThreadTaskQueueForTest(QueueType queue_type)
      : MainThreadTaskQueue(nullptr, QueueCreationParams(queue_type), nullptr) {
  }
  ~MainThreadTaskQueueForTest() {}
};

class MockWebFrameScheduler : public FakeWebFrameScheduler {
 public:
  MockWebFrameScheduler(bool is_page_visible,
                        bool is_frame_visible,
                        WebFrameScheduler::FrameType frame_type,
                        bool is_cross_origin,
                        bool is_exempt_from_throttling)
      : is_page_visible_(is_page_visible),
        is_frame_visible_(is_frame_visible),
        frame_type_(frame_type),
        is_cross_origin_(is_cross_origin),
        is_exempt_from_throttling_(is_exempt_from_throttling) {
    DCHECK(frame_type_ != FrameType::kMainFrame || !is_cross_origin);
  }

  bool IsCrossOrigin() const override { return is_cross_origin_; }
  bool IsExemptFromThrottling() const override {
    return is_exempt_from_throttling_;
  }
  bool IsFrameVisible() const override { return is_frame_visible_; }
  bool IsPageVisible() const override { return is_page_visible_; }
  FrameType GetFrameType() const override { return frame_type_; }

 private:
  bool is_page_visible_;
  bool is_frame_visible_;
  WebFrameScheduler::FrameType frame_type_;
  bool is_cross_origin_;
  bool is_exempt_from_throttling_;
};

}  // namespace

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

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  RendererMetricsHelper* metrics_helper_;  // NOT OWNED
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(RendererMetricsHelperTest);
};

TEST_F(RendererMetricsHelperTest, Metrics) {
  // QueueType::DEFAULT is checking sub-millisecond task aggregation,
  // FRAME_* tasks are checking normal task aggregation and other
  // queue types have a single task.
  RunTask(QueueType::DEFAULT, Milliseconds(1),
          base::TimeDelta::FromMicroseconds(700));
  RunTask(QueueType::DEFAULT, Milliseconds(2),
          base::TimeDelta::FromMicroseconds(700));
  RunTask(QueueType::DEFAULT, Milliseconds(3),
          base::TimeDelta::FromMicroseconds(700));

  RunTask(QueueType::DEFAULT_LOADING, Milliseconds(200),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::CONTROL, Milliseconds(400),
          base::TimeDelta::FromMilliseconds(30));
  RunTask(QueueType::DEFAULT_TIMER, Milliseconds(600),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::FRAME_LOADING, Milliseconds(800),
          base::TimeDelta::FromMilliseconds(70));
  RunTask(QueueType::FRAME_PAUSABLE, Milliseconds(1000),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::COMPOSITOR, Milliseconds(1200),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::TEST, Milliseconds(1600),
          base::TimeDelta::FromMilliseconds(85));

  scheduler_->SetRendererBackgrounded(true);

  RunTask(QueueType::CONTROL, Milliseconds(2000),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::FRAME_THROTTLEABLE, Milliseconds(2600),
          base::TimeDelta::FromMilliseconds(175));
  RunTask(QueueType::UNTHROTTLED, Milliseconds(2800),
          base::TimeDelta::FromMilliseconds(25));
  RunTask(QueueType::FRAME_LOADING, Milliseconds(3000),
          base::TimeDelta::FromMilliseconds(35));
  RunTask(QueueType::FRAME_THROTTLEABLE, Milliseconds(3200),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::COMPOSITOR, Milliseconds(3400),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::IDLE, Milliseconds(3600),
          base::TimeDelta::FromMilliseconds(50));
  RunTask(QueueType::FRAME_LOADING_CONTROL, Milliseconds(4000),
          base::TimeDelta::FromMilliseconds(5));
  RunTask(QueueType::CONTROL, Milliseconds(4200),
          base::TimeDelta::FromMilliseconds(20));
  RunTask(QueueType::FRAME_THROTTLEABLE, Milliseconds(4400),
          base::TimeDelta::FromMilliseconds(115));
  RunTask(QueueType::FRAME_PAUSABLE, Milliseconds(4600),
          base::TimeDelta::FromMilliseconds(175));
  RunTask(QueueType::IDLE, Milliseconds(5000),
          base::TimeDelta::FromMilliseconds(1600));

  std::vector<base::Bucket> expected_samples = {
      {static_cast<int>(QueueType::CONTROL), 75},
      {static_cast<int>(QueueType::DEFAULT), 2},
      {static_cast<int>(QueueType::DEFAULT_LOADING), 20},
      {static_cast<int>(QueueType::DEFAULT_TIMER), 5},
      {static_cast<int>(QueueType::UNTHROTTLED), 25},
      {static_cast<int>(QueueType::FRAME_LOADING), 105},
      {static_cast<int>(QueueType::COMPOSITOR), 45},
      {static_cast<int>(QueueType::IDLE), 1650},
      {static_cast<int>(QueueType::TEST), 85},
      {static_cast<int>(QueueType::FRAME_LOADING_CONTROL), 5},
      {static_cast<int>(QueueType::FRAME_THROTTLEABLE), 295},
      {static_cast<int>(QueueType::FRAME_PAUSABLE), 195}};
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType2"),
              testing::ContainerEq(expected_samples));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType2.Foreground"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(QueueType::CONTROL), 30),
                  Bucket(static_cast<int>(QueueType::DEFAULT), 2),
                  Bucket(static_cast<int>(QueueType::DEFAULT_LOADING), 20),
                  Bucket(static_cast<int>(QueueType::DEFAULT_TIMER), 5),
                  Bucket(static_cast<int>(QueueType::FRAME_LOADING), 70),
                  Bucket(static_cast<int>(QueueType::COMPOSITOR), 25),
                  Bucket(static_cast<int>(QueueType::TEST), 85),
                  Bucket(static_cast<int>(QueueType::FRAME_PAUSABLE), 20)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskDurationPerQueueType2.Background"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(QueueType::CONTROL), 45),
          Bucket(static_cast<int>(QueueType::UNTHROTTLED), 25),
          Bucket(static_cast<int>(QueueType::FRAME_LOADING), 35),
          Bucket(static_cast<int>(QueueType::FRAME_THROTTLEABLE), 295),
          Bucket(static_cast<int>(QueueType::FRAME_PAUSABLE), 175),
          Bucket(static_cast<int>(QueueType::COMPOSITOR), 20),
          Bucket(static_cast<int>(QueueType::IDLE), 1650),
          Bucket(static_cast<int>(QueueType::FRAME_LOADING_CONTROL), 5)));
}

TEST_F(RendererMetricsHelperTest, GetFrameTypeTest) {
  MockWebFrameScheduler frame1(
      true, true, WebFrameScheduler::FrameType::kMainFrame, false, false);
  EXPECT_EQ(GetFrameType(frame1), FrameType::MAIN_FRAME_VISIBLE);

  MockWebFrameScheduler frame2(
      true, false, WebFrameScheduler::FrameType::kSubframe, false, false);
  EXPECT_EQ(GetFrameType(frame2), FrameType::SAME_ORIGIN_HIDDEN);

  MockWebFrameScheduler frame3(
      true, false, WebFrameScheduler::FrameType::kSubframe, true, false);
  EXPECT_EQ(GetFrameType(frame3), FrameType::CROSS_ORIGIN_HIDDEN);

  MockWebFrameScheduler frame4(
      false, false, WebFrameScheduler::FrameType::kSubframe, false, false);
  EXPECT_EQ(GetFrameType(frame4), FrameType::SAME_ORIGIN_BACKGROUND);

  MockWebFrameScheduler frame5(
      false, false, WebFrameScheduler::FrameType::kMainFrame, false, true);
  DCHECK_EQ(GetFrameType(frame5), FrameType::MAIN_FRAME_BACKGROUND_EXEMPT);
}

// TODO(crbug.com/754656): Add tests for NthMinute and AfterNthMinute
// histograms.

// TODO(crbug.com/754656): Add tests for TaskDuration.Hidden/Visible histograms.

// TODO(crbug.com/754656): Add tests for non-TaskDuration histograms.

}  // namespace scheduler
}  // namespace blink
