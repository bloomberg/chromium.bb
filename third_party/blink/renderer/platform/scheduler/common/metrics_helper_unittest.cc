// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"

#include "base/task/sequence_manager/test/fake_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::sequence_manager::TaskQueue;

namespace blink {
namespace scheduler {

namespace {

using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;

class MetricsHelperForTest : public MetricsHelper {
 public:
  MetricsHelperForTest(WebThreadType thread_type)
      : MetricsHelper(thread_type) {}
  ~MetricsHelperForTest() = default;

  using MetricsHelper::RecordCommonTaskMetrics;
};

}  // namespace

TEST(MetricsHelperTest, TaskDurationPerThreadType) {
  base::HistogramTester histogram_tester;

  MetricsHelperForTest main_thread_metrics(WebThreadType::kMainThread);
  MetricsHelperForTest compositor_metrics(WebThreadType::kCompositorThread);
  MetricsHelperForTest worker_metrics(WebThreadType::kUnspecifiedWorkerThread);

  main_thread_metrics.RecordCommonTaskMetrics(
      nullptr, FakeTask(),
      FakeTaskTiming(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
                     base::TimeTicks() + base::TimeDelta::FromSeconds(50),
                     base::ThreadTicks(),
                     base::ThreadTicks() + base::TimeDelta::FromSeconds(15)));
  compositor_metrics.RecordCommonTaskMetrics(
      nullptr, FakeTask(),
      FakeTaskTiming(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
                     base::TimeTicks() + base::TimeDelta::FromSeconds(80),
                     base::ThreadTicks(),
                     base::ThreadTicks() + base::TimeDelta::FromSeconds(5)));
  compositor_metrics.RecordCommonTaskMetrics(
      nullptr, FakeTask(),
      FakeTaskTiming(base::TimeTicks() + base::TimeDelta::FromSeconds(100),
                     base::TimeTicks() + base::TimeDelta::FromSeconds(200)));
  worker_metrics.RecordCommonTaskMetrics(
      nullptr, FakeTask(),
      FakeTaskTiming(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
                     base::TimeTicks() + base::TimeDelta::FromSeconds(125),
                     base::ThreadTicks(),
                     base::ThreadTicks() + base::TimeDelta::FromSeconds(25)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "RendererScheduler.TaskDurationPerThreadType2"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<int>(WebThreadType::kMainThread), 40),
          base::Bucket(static_cast<int>(WebThreadType::kCompositorThread), 170),
          base::Bucket(
              static_cast<int>(WebThreadType::kUnspecifiedWorkerThread), 115)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "RendererScheduler.TaskCPUDurationPerThreadType2"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<int>(WebThreadType::kMainThread), 15),
          base::Bucket(static_cast<int>(WebThreadType::kCompositorThread), 5),
          base::Bucket(
              static_cast<int>(WebThreadType::kUnspecifiedWorkerThread), 25)));
}

}  // namespace scheduler
}  // namespace blink
