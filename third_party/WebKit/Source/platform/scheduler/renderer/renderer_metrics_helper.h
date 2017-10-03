// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/thread_load_tracker.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"
#include "platform/scheduler/renderer/task_duration_metric_reporter.h"

namespace blink {

class WebFrameScheduler;
namespace scheduler {

class MainThreadTaskQueue;
class RendererSchedulerImpl;

// This enum is used for histogram and should not be renumbered.
// This enum should be kept in sync with FrameThrottlingState and
// FrameOriginState.
enum class FrameType {
  MAIN_FRAME_VISIBLE = 0,
  MAIN_FRAME_HIDDEN = 1,
  MAIN_FRAME_BACKGROUND = 2,
  MAIN_FRAME_BACKGROUND_EXEMPT = 3,
  SAME_ORIGIN_VISIBLE = 4,
  SAME_ORIGIN_HIDDEN = 5,
  SAME_ORIGIN_BACKGROUND = 6,
  SAME_ORIGIN_BACKGROUND_EXEMPT = 7,
  CROSS_ORIGIN_VISIBLE = 8,
  CROSS_ORIGIN_HIDDEN = 9,
  CROSS_ORIGIN_BACKGROUND = 10,
  CROSS_ORIGIN_BACKGROUND_EXEMPT = 11,

  COUNT = 12
};

PLATFORM_EXPORT FrameType
GetFrameType(const WebFrameScheduler& frame_scheduler);

// Helper class to take care of metrics on behalf of RendererScheduler.
// This class should be used only on the main thread.
class PLATFORM_EXPORT RendererMetricsHelper {
 public:
  RendererMetricsHelper(RendererSchedulerImpl* renderer_scheduler,
                        base::TimeTicks now,
                        bool renderer_backgrounded);
  ~RendererMetricsHelper();

  void RecordTaskMetrics(MainThreadTaskQueue* queue,
                         base::TimeTicks start_time,
                         base::TimeTicks end_time);

  void OnRendererForegrounded(base::TimeTicks now);
  void OnRendererBackgrounded(base::TimeTicks now);
  void OnRendererShutdown(base::TimeTicks now);

  void RecordMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordForegroundMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordBackgroundMainThreadTaskLoad(base::TimeTicks time, double load);

 private:
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  base::Optional<base::TimeTicks> last_reported_task_;

  ThreadLoadTracker main_thread_load_tracker;
  ThreadLoadTracker background_main_thread_load_tracker;
  ThreadLoadTracker foreground_main_thread_load_tracker;

  using TaskDurationPerQueueTypeMetricReporter =
      TaskDurationMetricReporter<MainThreadTaskQueue::QueueType>;

  TaskDurationPerQueueTypeMetricReporter task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter foreground_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      foreground_first_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      foreground_second_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      foreground_third_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      foreground_after_third_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter background_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_first_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_second_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_third_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_fourth_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_fifth_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter
      background_after_fifth_minute_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter hidden_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter visible_task_duration_reporter;
  TaskDurationPerQueueTypeMetricReporter hidden_music_task_duration_reporter;

  TaskDurationMetricReporter<FrameType> frame_type_duration_reporter;

  DISALLOW_COPY_AND_ASSIGN(RendererMetricsHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
