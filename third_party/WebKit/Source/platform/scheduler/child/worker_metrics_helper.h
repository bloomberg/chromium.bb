// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_METRICS_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_METRICS_HELPER_H_

#include "platform/scheduler/child/metrics_helper.h"
#include "platform/scheduler/child/worker_task_queue.h"
#include "platform/scheduler/util/thread_load_tracker.h"
#include "public/platform/TaskType.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT WorkerMetricsHelper : public MetricsHelper {
 public:
  WorkerMetricsHelper();
  ~WorkerMetricsHelper();

  void RecordTaskMetrics(WorkerTaskQueue* queue,
                         const TaskQueue::Task& task,
                         base::TimeTicks start_time,
                         base::TimeTicks end_time,
                         base::Optional<base::TimeDelta> thread_time);

  using MetricsHelper::SetThreadType;

 private:
  TaskDurationMetricReporter<TaskType>
      dedicated_worker_per_task_type_duration_reporter_;
  TaskDurationMetricReporter<TaskType>
      dedicated_worker_per_task_type_cpu_duration_reporter_;

  DISALLOW_COPY_AND_ASSIGN(WorkerMetricsHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_METRICS_HELPER_H_
