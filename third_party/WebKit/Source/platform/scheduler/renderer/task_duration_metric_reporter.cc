// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/task_duration_metric_reporter.h"

#include "base/metrics/histogram.h"

namespace blink {
namespace scheduler {

TaskDurationMetricReporter::TaskDurationMetricReporter(const char* metric_name)
    : TaskDurationMetricReporter(base::Histogram::FactoryGet(
          metric_name,
          1,
          static_cast<int>(MainThreadTaskQueue::QueueType::COUNT),
          static_cast<int>(MainThreadTaskQueue::QueueType::COUNT) + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag)) {}

TaskDurationMetricReporter::TaskDurationMetricReporter(
    base::HistogramBase* histogram)
    : task_duration_per_queue_type_histogram_(histogram) {}

TaskDurationMetricReporter::~TaskDurationMetricReporter() {}

void TaskDurationMetricReporter::RecordTask(
    MainThreadTaskQueue::QueueType queue_type,
    base::TimeDelta duration) {
  // Report only whole milliseconds to avoid overflow.
  base::TimeDelta& unreported_duration =
      unreported_task_duration_[static_cast<int>(queue_type)];
  unreported_duration += duration;
  int64_t milliseconds = unreported_duration.InMilliseconds();
  if (milliseconds > 0) {
    unreported_duration -= base::TimeDelta::FromMilliseconds(milliseconds);
    task_duration_per_queue_type_histogram_->AddCount(
        static_cast<int>(queue_type), static_cast<int>(milliseconds));
  }
}

}  // namespace scheduler
}  // namespace blink
