// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_metrics_helper.h"

namespace blink {
namespace scheduler {

WorkerMetricsHelper::WorkerMetricsHelper()
    : MetricsHelper(ThreadType::kUnspecifiedWorkerThread) {}

WorkerMetricsHelper::~WorkerMetricsHelper() {}

void WorkerMetricsHelper::RecordTaskMetrics(WorkerTaskQueue* queue,
                                            const TaskQueue::Task& task,
                                            base::TimeTicks start_time,
                                            base::TimeTicks end_time) {
  MetricsHelper::RecordCommonTaskMetrics(queue, task, start_time, end_time);
}

}  // namespace scheduler
}  // namespace blink
