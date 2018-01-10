// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/time/time.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/util/task_duration_metric_reporter.h"
#include "platform/scheduler/util/thread_type.h"

namespace blink {
namespace scheduler {

class TaskQueue;

// Helper class to take care of task metrics shared between main thread
// and worker threads of the renderer process, including per-thread
// task metrics.
//
// Each thread-specific scheduler should have its own subclass of MetricsHelper
// (RendererMetricsHelper, WorkerMetricsHelper, etc) and should call
// RecordCommonTaskMetrics manually.
// Note that this is code reuse, not data reuse -- each thread should have its
// own instantiation of this class.
class PLATFORM_EXPORT MetricsHelper {
 public:
  explicit MetricsHelper(ThreadType thread_type);
  ~MetricsHelper();

 protected:
  // Record task metrics which are shared between threads.
  void RecordCommonTaskMetrics(TaskQueue* queue,
                               const TaskQueue::Task& task,
                               base::TimeTicks start_time,
                               base::TimeTicks end_time);

 private:
  ThreadType thread_type_;

  TaskDurationMetricReporter<ThreadType> thread_task_duration_reporter_;

  DISALLOW_COPY_AND_ASSIGN(MetricsHelper);
};

}  // namespace scheduler
}  // namespace blink
