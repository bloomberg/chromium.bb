// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/child/metrics_helper.h"
#include "platform/scheduler/renderer/frame_status.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"
#include "platform/scheduler/renderer/use_case.h"
#include "platform/scheduler/util/task_duration_metric_reporter.h"
#include "platform/scheduler/util/thread_load_tracker.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebThreadType.h"

namespace blink {
namespace scheduler {

enum class MainThreadTaskLoadState;
class MainThreadTaskQueue;
class RendererSchedulerImpl;

// This enum is used for histogram and should not be renumbered.
// It tracks the following possible transitions:
// -> kBackgrounded (-> [STOPPED_* -> kResumed])? -> kForegrounded
enum class BackgroundedRendererTransition {
  // Renderer is backgrounded
  kBackgrounded = 0,
  // Renderer is stopped after being backgrounded for a while
  kStoppedAfterDelay = 1,
  // Renderer is stopped due to critical resources, reserved for future use.
  kStoppedDueToCriticalResources = 2,
  // Renderer is resumed after being stopped
  kResumed = 3,
  // Renderer is foregrounded
  kForegrounded = 4,

  kCount = 5
};

// Helper class to take care of metrics on behalf of RendererScheduler.
// This class should be used only on the main thread.
class PLATFORM_EXPORT RendererMetricsHelper : public MetricsHelper {
 public:
  static void RecordBackgroundedTransition(
      BackgroundedRendererTransition transition);

  RendererMetricsHelper(RendererSchedulerImpl* renderer_scheduler,
                        base::TimeTicks now,
                        bool renderer_backgrounded);
  ~RendererMetricsHelper();

  void RecordTaskMetrics(MainThreadTaskQueue* queue,
                         const TaskQueue::Task& task,
                         base::TimeTicks start_time,
                         base::TimeTicks end_time,
                         base::Optional<base::TimeDelta> thread_time);

  void OnRendererForegrounded(base::TimeTicks now);
  void OnRendererBackgrounded(base::TimeTicks now);
  void OnRendererShutdown(base::TimeTicks now);

  void RecordMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordForegroundMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordBackgroundMainThreadTaskLoad(base::TimeTicks time, double load);

  void ResetForTest(base::TimeTicks now);

 private:
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  base::Optional<base::TimeTicks> last_reported_task_;

  ThreadLoadTracker main_thread_load_tracker_;
  ThreadLoadTracker background_main_thread_load_tracker_;
  ThreadLoadTracker foreground_main_thread_load_tracker_;

  using TaskDurationPerQueueTypeMetricReporter =
      TaskDurationMetricReporter<MainThreadTaskQueue::QueueType>;

  struct PerQueueTypeDurationReporters {
    PerQueueTypeDurationReporters();

    TaskDurationPerQueueTypeMetricReporter overall;
    TaskDurationPerQueueTypeMetricReporter foreground;
    TaskDurationPerQueueTypeMetricReporter foreground_first_minute;
    TaskDurationPerQueueTypeMetricReporter foreground_second_minute;
    TaskDurationPerQueueTypeMetricReporter foreground_third_minute;
    TaskDurationPerQueueTypeMetricReporter foreground_after_third_minute;
    TaskDurationPerQueueTypeMetricReporter background;
    TaskDurationPerQueueTypeMetricReporter background_first_minute;
    TaskDurationPerQueueTypeMetricReporter background_second_minute;
    TaskDurationPerQueueTypeMetricReporter background_third_minute;
    TaskDurationPerQueueTypeMetricReporter background_fourth_minute;
    TaskDurationPerQueueTypeMetricReporter background_fifth_minute;
    TaskDurationPerQueueTypeMetricReporter background_after_fifth_minute;
    TaskDurationPerQueueTypeMetricReporter
        background_keep_active_after_fifth_minute;
    TaskDurationPerQueueTypeMetricReporter hidden;
    TaskDurationPerQueueTypeMetricReporter visible;
    TaskDurationPerQueueTypeMetricReporter hidden_music;
  };

  PerQueueTypeDurationReporters per_queue_type_reporters_;

  TaskDurationMetricReporter<FrameStatus> per_frame_status_duration_reporter_;

  using TaskDurationPerTaskTypeMetricReporter =
      TaskDurationMetricReporter<TaskType>;

  TaskDurationPerTaskTypeMetricReporter per_task_type_duration_reporter_;

  // The next three reporters are used to report the duration per task type
  // split by renderer scheduler use case (check use_case.h for reference):
  // None, Loading, and User Input (aggregation of multiple input-handling
  // related use cases).
  TaskDurationPerTaskTypeMetricReporter
      no_use_case_per_task_type_duration_reporter_;
  TaskDurationPerTaskTypeMetricReporter
      loading_per_task_type_duration_reporter_;
  TaskDurationPerTaskTypeMetricReporter
      input_handling_per_task_type_duration_reporter_;

  TaskDurationPerTaskTypeMetricReporter
      foreground_per_task_type_duration_reporter_;
  TaskDurationPerTaskTypeMetricReporter
      background_per_task_type_duration_reporter_;

  TaskDurationMetricReporter<UseCase> per_task_use_case_duration_reporter_;

  MainThreadTaskLoadState main_thread_task_load_state_;

  DISALLOW_COPY_AND_ASSIGN(RendererMetricsHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
