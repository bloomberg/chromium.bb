// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_metrics_helper.h"

namespace blink {
namespace scheduler {

WorkerMetricsHelper::WorkerMetricsHelper()
    : MetricsHelper(ThreadType::kUnspecifiedWorkerThread),
      dedicated_worker_per_task_type_duration_reporter_(
          "RendererScheduler.TaskDurationPerTaskType.DedicatedWorker"),
      dedicated_worker_per_task_type_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerTaskType.DedicatedWorker") {}

WorkerMetricsHelper::~WorkerMetricsHelper() {}

void WorkerMetricsHelper::RecordTaskMetrics(
    WorkerTaskQueue* queue,
    const TaskQueue::Task& task,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    base::Optional<base::TimeDelta> thread_time) {
  if (ShouldDiscardTask(queue, task, start_time, end_time, thread_time))
    return;

  MetricsHelper::RecordCommonTaskMetrics(queue, task, start_time, end_time,
                                         thread_time);

  if (thread_type_ == ThreadType::kDedicatedWorkerThread) {
    base::Optional<TaskType> task_type = task.task_type();
    dedicated_worker_per_task_type_duration_reporter_.RecordTask(
        task_type ? task_type.value() : static_cast<TaskType>(0),
        end_time - start_time);
    if (thread_time) {
      dedicated_worker_per_task_type_cpu_duration_reporter_.RecordTask(
          task_type ? task_type.value() : static_cast<TaskType>(0),
          thread_time.value());
    }
  }
}

}  // namespace scheduler
}  // namespace blink
