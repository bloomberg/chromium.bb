// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_DURATION_METRIC_REPORTER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_DURATION_METRIC_REPORTER_H_

#include <array>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"

namespace base {
class HistogramBase;
}

namespace blink {
namespace scheduler {

// A helper class to report task duration split by queue type.
// Aggregates small tasks internally and reports only whole milliseconds.
class PLATFORM_EXPORT TaskDurationMetricReporter {
 public:
  explicit TaskDurationMetricReporter(const char* metric_name);
  ~TaskDurationMetricReporter();

  void RecordTask(MainThreadTaskQueue::QueueType queue_type,
                  base::TimeDelta time_duration);

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskDurationMetricReporterTest, Test);

  TaskDurationMetricReporter(base::HistogramBase* histogram);

  std::array<base::TimeDelta,
             static_cast<size_t>(MainThreadTaskQueue::QueueType::COUNT)>
      unreported_task_duration_;
  base::HistogramBase* task_duration_per_queue_type_histogram_;

  DISALLOW_COPY_AND_ASSIGN(TaskDurationMetricReporter);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_DURATION_METRIC_REPORTER_H_
