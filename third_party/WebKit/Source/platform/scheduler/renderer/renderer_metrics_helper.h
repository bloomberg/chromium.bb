// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"
#include "platform/scheduler/renderer/task_duration_metric_reporter.h"
#include "platform/scheduler/util/thread_load_tracker.h"

namespace blink {

class WebFrameScheduler;
namespace scheduler {

enum class MainThreadTaskLoadState;
class MainThreadTaskQueue;
class RendererSchedulerImpl;

// This enum is used for histogram and should not be renumbered.
// This enum should be kept in sync with FrameThrottlingState and
// FrameOriginState.
//
// There are three main states:
// VISIBLE describes frames which are visible to the user (both page and frame
// are visible).
// Without this service frame would have had kBackgrounded state.
// HIDDEN describes frames which are out of viewport but the page is visible
// to the user.
// BACKGROUND describes frames in background pages.
//
// There are four auxillary states:
// VISIBLE_SERVICE describes frames which are treated as visible to the user
// but it is a service (e.g. audio) which forces the page to be foregrounded.
// HIDDEN_SERVICE describes offscreen frames in pages which are treated as
// foregrounded due to a presence of a service (e.g. audio playing).
// BACKGROUND_EXEMPT_SELF describes background frames which are
// exempted from background throttling due to a special conditions being met
// for this frame.
// BACKGROUND_EXEMPT_kOther describes background frames which are exempted from
// background throttling due to other frames granting an exemption for
// the whole page.
//
// Note that all these seven states are disjoint, e.g, when calculating
// a metric for background BACKGROUND, BACKGROUND_EXEMPT_SELF and
// BACKGROUND_EXEMPT_kOther should be added together.
enum class FrameType {
  // Used to describe a task queue which doesn't have a frame associated
  // (e.g. global task queue).
  kNone = 0,

  // This frame was detached and does not have origin or visibility status
  // anymore.
  kDetached = 1,

  kSpecialCasesCount = 2,

  kMainFrameVisible = 2,
  kMainFrameVisibleService = 3,
  kMainFrameHidden = 4,
  kMainFrameHiddenService = 5,
  kMainFrameBackground = 6,
  kMainFrameBackgroundExemptSelf = 7,
  kMainFrameBackgroundExemptOther = 8,

  kSameOriginVisible = 9,
  kSameOriginVisibleService = 10,
  kSameOriginHidden = 11,
  kSameOriginHiddenService = 12,
  kSameOriginBackground = 13,
  kSameOriginBackgroundExemptSelf = 14,
  kSameOriginBackgroundExemptOther = 15,

  kCrossOriginVisible = 16,
  kCrossOriginVisibleService = 17,
  kCrossOriginHidden = 18,
  kCrossOriginHiddenService = 19,
  kCrossOriginBackground = 20,
  kCrossOriginBackgroundExemptSelf = 21,
  kCrossOriginBackgroundExemptOther = 22,

  kCount = 23
};

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

PLATFORM_EXPORT FrameType GetFrameType(WebFrameScheduler* frame_scheduler);

// Helper class to take care of metrics on behalf of RendererScheduler.
// This class should be used only on the main thread.
class PLATFORM_EXPORT RendererMetricsHelper {
 public:
  static void RecordBackgroundedTransition(
      BackgroundedRendererTransition transition);

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
  MainThreadTaskLoadState main_thread_task_load_state;

  DISALLOW_COPY_AND_ASSIGN(RendererMetricsHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_RENDERER_METRICS_HELPER_H_
