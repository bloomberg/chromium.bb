// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/metrics_helper.h"

namespace blink {
namespace scheduler {

MetricsHelper::MetricsHelper(ThreadType thread_type)
    : thread_type_(thread_type),
      thread_task_duration_reporter_(
          "RendererScheduler.TaskDurationPerThreadType") {}

MetricsHelper::~MetricsHelper() {}

void MetricsHelper::RecordCommonTaskMetrics(TaskQueue* queue,
                                            const TaskQueue::Task& task,
                                            base::TimeTicks start_time,
                                            base::TimeTicks end_time) {
  thread_task_duration_reporter_.RecordTask(thread_type_,
                                            end_time - start_time);
}

void MetricsHelper::SetThreadType(ThreadType thread_type) {
  thread_type_ = thread_type;
}

}  // namespace scheduler
}  // namespace blink
