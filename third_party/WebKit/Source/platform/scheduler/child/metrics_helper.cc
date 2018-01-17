// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/metrics_helper.h"

namespace blink {
namespace scheduler {

namespace {

// Threshold for discarding ultra-long tasks. It is assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep on the middle of the
// task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold =
    base::TimeDelta::FromSeconds(30);

}  // namespace

MetricsHelper::MetricsHelper(ThreadType thread_type)
    : thread_type_(thread_type),
      thread_task_duration_reporter_(
          "RendererScheduler.TaskDurationPerThreadType"),
      thread_task_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerThreadType") {}

MetricsHelper::~MetricsHelper() {}

bool MetricsHelper::ShouldDiscardTask(
    TaskQueue* queue,
    const TaskQueue::Task& task,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    base::Optional<base::TimeDelta> thread_time) {
  // TODO(altimin): Investigate the relationship between thread time and
  // wall time for discarded tasks.
  return end_time - start_time > kLongTaskDiscardingThreshold;
}

void MetricsHelper::RecordCommonTaskMetrics(
    TaskQueue* queue,
    const TaskQueue::Task& task,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    base::Optional<base::TimeDelta> thread_time) {
  thread_task_duration_reporter_.RecordTask(thread_type_,
                                            end_time - start_time);
  if (thread_time) {
    thread_task_cpu_duration_reporter_.RecordTask(thread_type_,
                                                  thread_time.value());
  }
}

void MetricsHelper::SetThreadType(ThreadType thread_type) {
  thread_type_ = thread_type;
}

}  // namespace scheduler
}  // namespace blink
