// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_metrics_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/resource_coordinator/RendererResourceCoordinator.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "public/platform/scheduler/renderer_process_type.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace blink {
namespace scheduler {

#define DURATION_PER_QUEUE_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerQueueType2"
#define COUNT_PER_QUEUE_TYPE_METRIC_NAME \
  "RendererScheduler.TaskCountPerQueueType"
#define MAIN_THREAD_LOAD_METRIC_NAME "RendererScheduler.RendererMainThreadLoad5"
#define EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME \
  MAIN_THREAD_LOAD_METRIC_NAME ".Extension"
#define DURATION_PER_FRAME_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerFrameType2"
#define DURATION_PER_TASK_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerTaskType"
#define COUNT_PER_FRAME_METRIC_NAME "RendererScheduler.TaskCountPerFrameType"

enum class MainThreadTaskLoadState { kLow, kHigh, kUnknown };

namespace {

constexpr base::TimeDelta kThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromSeconds(1);
// Threshold for discarding ultra-long tasks. Is it assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep in the middle
// of the task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kLongIdlePeriodDiscardingThreshold =
    base::TimeDelta::FromMinutes(3);

enum class FrameThrottlingState {
  kVisible = 0,
  kVisibleService = 1,
  kHidden = 2,
  kHiddenService = 3,
  kBackground = 4,
  kBackgroundExemptSelf = 5,
  kBackgroundExemptOther = 6,

  kCount = 7
};

enum class FrameOriginState {
  kMainFrame = 0,
  kSameOrigin = 1,
  kCrossOrigin = 2,

  kCount = 3
};

FrameThrottlingState GetFrameThrottlingState(
    const WebFrameScheduler& frame_scheduler) {
  if (frame_scheduler.IsPageVisible()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisible;
    return FrameThrottlingState::kHidden;
  }

  WebViewScheduler* web_view_scheduler = frame_scheduler.GetWebViewScheduler();
  if (web_view_scheduler && web_view_scheduler->IsPlayingAudio()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisibleService;
    return FrameThrottlingState::kHiddenService;
  }

  if (frame_scheduler.IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptSelf;

  if (web_view_scheduler &&
      web_view_scheduler->IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptOther;

  return FrameThrottlingState::kBackground;
}

FrameOriginState GetFrameOriginState(const WebFrameScheduler& frame_scheduler) {
  if (frame_scheduler.GetFrameType() ==
      WebFrameScheduler::FrameType::kMainFrame) {
    return FrameOriginState::kMainFrame;
  }
  if (frame_scheduler.IsCrossOrigin())
    return FrameOriginState::kCrossOrigin;
  return FrameOriginState::kSameOrigin;
}

}  // namespace

FrameType GetFrameType(WebFrameScheduler* frame_scheduler) {
  if (!frame_scheduler)
    return FrameType::kNone;
  FrameThrottlingState throttling_state =
      GetFrameThrottlingState(*frame_scheduler);
  FrameOriginState origin_state = GetFrameOriginState(*frame_scheduler);
  return static_cast<FrameType>(
      static_cast<int>(FrameType::kSpecialCasesCount) +
      static_cast<int>(origin_state) *
          static_cast<int>(FrameThrottlingState::kCount) +
      static_cast<int>(throttling_state));
}

RendererMetricsHelper::RendererMetricsHelper(
    RendererSchedulerImpl* renderer_scheduler,
    base::TimeTicks now,
    bool renderer_backgrounded)
    : renderer_scheduler_(renderer_scheduler),
      main_thread_load_tracker(
          now,
          base::Bind(&RendererMetricsHelper::RecordMainThreadTaskLoad,
                     base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      background_main_thread_load_tracker(
          now,
          base::Bind(&RendererMetricsHelper::RecordBackgroundMainThreadTaskLoad,
                     base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      foreground_main_thread_load_tracker(
          now,
          base::Bind(&RendererMetricsHelper::RecordForegroundMainThreadTaskLoad,
                     base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME),
      foreground_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground"),
      foreground_first_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground.FirstMinute"),
      foreground_second_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground.SecondMinute"),
      foreground_third_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground.ThirdMinute"),
      foreground_after_third_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground.AfterThirdMinute"),
      background_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background"),
      background_first_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.FirstMinute"),
      background_second_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.SecondMinute"),
      background_third_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.ThirdMinute"),
      background_fourth_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.FourthMinute"),
      background_fifth_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.FifthMinute"),
      background_after_fifth_minute_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background.AfterFifthMinute"),
      hidden_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Hidden"),
      visible_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Visible"),
      hidden_music_per_queue_type_task_duration_reporter(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME ".HiddenMusic"),
      per_frame_type_duration_reporter(DURATION_PER_FRAME_TYPE_METRIC_NAME),
      per_task_type_duration_reporter(DURATION_PER_TASK_TYPE_METRIC_NAME),
      main_thread_task_duration_reporter(
          "RendererScheduler.TaskDurationPerThreadType"),
      main_thread_task_load_state(MainThreadTaskLoadState::kUnknown) {
  main_thread_load_tracker.Resume(now);
  if (renderer_backgrounded) {
    background_main_thread_load_tracker.Resume(now);
  } else {
    foreground_main_thread_load_tracker.Resume(now);
  }
}

RendererMetricsHelper::~RendererMetricsHelper() {}

void RendererMetricsHelper::OnRendererForegrounded(base::TimeTicks now) {
  foreground_main_thread_load_tracker.Resume(now);
  background_main_thread_load_tracker.Pause(now);
}

void RendererMetricsHelper::OnRendererBackgrounded(base::TimeTicks now) {
  foreground_main_thread_load_tracker.Pause(now);
  background_main_thread_load_tracker.Resume(now);
}

void RendererMetricsHelper::OnRendererShutdown(base::TimeTicks now) {
  foreground_main_thread_load_tracker.RecordIdle(now);
  background_main_thread_load_tracker.RecordIdle(now);
  main_thread_load_tracker.RecordIdle(now);
}

namespace {

// Calculates the length of the intersection of two given time intervals.
base::TimeDelta DurationOfIntervalOverlap(base::TimeTicks start1,
                                          base::TimeTicks end1,
                                          base::TimeTicks start2,
                                          base::TimeTicks end2) {
  DCHECK_LE(start1, end1);
  DCHECK_LE(start2, end2);
  return std::max(std::min(end1, end2) - std::max(start1, start2),
                  base::TimeDelta());
}

}  // namespace

void RendererMetricsHelper::RecordTaskMetrics(MainThreadTaskQueue* queue,
                                              const TaskQueue::Task& task,
                                              base::TimeTicks start_time,
                                              base::TimeTicks end_time) {
  MainThreadTaskQueue::QueueType queue_type = queue->queue_type();
  base::TimeDelta duration = end_time - start_time;
  if (duration > kLongTaskDiscardingThreshold)
    return;

  // Discard anomalously long idle periods.
  if (last_reported_task_ && start_time - last_reported_task_.value() >
                                 kLongIdlePeriodDiscardingThreshold) {
    main_thread_load_tracker.Reset(end_time);
    foreground_main_thread_load_tracker.Reset(end_time);
    background_main_thread_load_tracker.Reset(end_time);
    return;
  }

  last_reported_task_ = end_time;

  UMA_HISTOGRAM_CUSTOM_COUNTS("RendererScheduler.TaskTime2",
                              duration.InMicroseconds(), 1, 1000 * 1000, 50);

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  main_thread_load_tracker.RecordTaskTime(start_time, end_time);
  foreground_main_thread_load_tracker.RecordTaskTime(start_time, end_time);
  background_main_thread_load_tracker.RecordTaskTime(start_time, end_time);

  UMA_HISTOGRAM_ENUMERATION(COUNT_PER_QUEUE_TYPE_METRIC_NAME, queue_type,
                            MainThreadTaskQueue::QueueType::kCount);

  if (duration >= base::TimeDelta::FromMilliseconds(16)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan16ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(50)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan50ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(100)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan100ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(150)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan150ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromSeconds(1)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan1s",
                              queue_type,
                              MainThreadTaskQueue::QueueType::kCount);
  }

  main_thread_task_duration_reporter.RecordTask(ThreadType::kMainThread,
                                                duration);
  per_queue_type_task_duration_reporter.RecordTask(queue_type, duration);

  if (renderer_scheduler_->main_thread_only().renderer_backgrounded) {
    background_per_queue_type_task_duration_reporter.RecordTask(queue_type,
                                                                duration);

    // Collect detailed breakdown for first five minutes given that we stop
    // timers on mobile after five minutes.
    base::TimeTicks backgrounded_at =
        renderer_scheduler_->main_thread_only().background_status_changed_at;

    background_first_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time, backgrounded_at,
                        backgrounded_at + base::TimeDelta::FromMinutes(1)));

    background_second_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(1),
                        backgrounded_at + base::TimeDelta::FromMinutes(2)));

    background_third_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(2),
                        backgrounded_at + base::TimeDelta::FromMinutes(3)));

    background_fourth_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(3),
                        backgrounded_at + base::TimeDelta::FromMinutes(4)));

    background_fifth_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(4),
                        backgrounded_at + base::TimeDelta::FromMinutes(5)));

    background_after_fifth_minute_per_queue_type_task_duration_reporter
        .RecordTask(
            queue_type,
            DurationOfIntervalOverlap(
                start_time, end_time,
                backgrounded_at + base::TimeDelta::FromMinutes(5),
                std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                         end_time)));
  } else {
    foreground_per_queue_type_task_duration_reporter.RecordTask(queue_type,
                                                                duration);

    // For foreground tabs we do not expect such a notable difference as it is
    // the case with background tabs, so we limit breakdown to three minutes.
    base::TimeTicks foregrounded_at =
        renderer_scheduler_->main_thread_only().background_status_changed_at;

    foreground_first_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time, foregrounded_at,
                        foregrounded_at + base::TimeDelta::FromMinutes(1)));

    foreground_second_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        foregrounded_at + base::TimeDelta::FromMinutes(1),
                        foregrounded_at + base::TimeDelta::FromMinutes(2)));

    foreground_third_minute_per_queue_type_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        foregrounded_at + base::TimeDelta::FromMinutes(2),
                        foregrounded_at + base::TimeDelta::FromMinutes(3)));

    foreground_after_third_minute_per_queue_type_task_duration_reporter
        .RecordTask(
            queue_type,
            DurationOfIntervalOverlap(
                start_time, end_time,
                foregrounded_at + base::TimeDelta::FromMinutes(3),
                std::max(foregrounded_at + base::TimeDelta::FromMinutes(3),
                         end_time)));
  }

  if (renderer_scheduler_->main_thread_only().renderer_hidden) {
    hidden_per_queue_type_task_duration_reporter.RecordTask(queue_type,
                                                            duration);

    if (renderer_scheduler_->ShouldDisableThrottlingBecauseOfAudio(
            start_time)) {
      hidden_music_per_queue_type_task_duration_reporter.RecordTask(queue_type,
                                                                    duration);
    }
  } else {
    visible_per_queue_type_task_duration_reporter.RecordTask(queue_type,
                                                             duration);
  }

  FrameType frame_type = GetFrameType(queue->GetFrameScheduler());
  per_frame_type_duration_reporter.RecordTask(frame_type, duration);
  UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME, frame_type,
                            FrameType::kCount);
  if (duration >= base::TimeDelta::FromMilliseconds(16)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan16ms",
                              frame_type, FrameType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(50)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan50ms",
                              frame_type, FrameType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(100)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan100ms",
                              frame_type, FrameType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(150)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan150ms",
                              frame_type, FrameType::kCount);
  }

  if (duration >= base::TimeDelta::FromSeconds(1)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan1s",
                              frame_type, FrameType::kCount);
  }

  base::Optional<TaskType> task_type = task.task_type();
  per_task_type_duration_reporter.RecordTask(
      task_type ? task_type.value() : static_cast<TaskType>(0), duration);
}

void RendererMetricsHelper::RecordMainThreadTaskLoad(base::TimeTicks time,
                                                     double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  if (::resource_coordinator::IsPageAlmostIdleSignalEnabled()) {
    static const int main_thread_task_load_low_threshold =
        ::resource_coordinator::GetMainThreadTaskLoadLowThreshold();

    // Avoid sending duplicate IPCs when the state doesn't change.
    if (load_percentage <= main_thread_task_load_low_threshold &&
        main_thread_task_load_state != MainThreadTaskLoadState::kLow) {
      RendererResourceCoordinator::Get().SetMainThreadTaskLoadIsLow(true);
      main_thread_task_load_state = MainThreadTaskLoadState::kLow;
    } else if (load_percentage > main_thread_task_load_low_threshold &&
               main_thread_task_load_state != MainThreadTaskLoadState::kHigh) {
      RendererResourceCoordinator::Get().SetMainThreadTaskLoadIsLow(false);
      main_thread_task_load_state = MainThreadTaskLoadState::kHigh;
    }
  }

  UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME, load_percentage);

  if (renderer_scheduler_->main_thread_only().process_type ==
      RendererProcessType::kExtensionRenderer) {
    UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME,
                             load_percentage);
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad", load_percentage);
}

void RendererMetricsHelper::RecordForegroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  switch (renderer_scheduler_->main_thread_only().process_type) {
    case RendererProcessType::kExtensionRenderer:
      UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME
                               ".Foreground",
                               load_percentage);
      break;
    case RendererProcessType::kRenderer:
      UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME ".Foreground",
                               load_percentage);

      if (time - renderer_scheduler_->main_thread_only()
                     .background_status_changed_at >
          base::TimeDelta::FromMinutes(1)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Foreground.AfterFirstMinute",
                                 load_percentage);
      }
      break;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad.Foreground",
                 load_percentage);
}

void RendererMetricsHelper::RecordBackgroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  switch (renderer_scheduler_->main_thread_only().process_type) {
    case RendererProcessType::kExtensionRenderer:
      UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME
                               ".Background",
                               load_percentage);
      break;
    case RendererProcessType::kRenderer:
      UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME ".Background",
                               load_percentage);

      if (time - renderer_scheduler_->main_thread_only()
                     .background_status_changed_at >
          base::TimeDelta::FromMinutes(1)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterFirstMinute",
                                 load_percentage);
      }
      break;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.RendererMainThreadLoad.Background",
                 load_percentage);
}

// static
void RendererMetricsHelper::RecordBackgroundedTransition(
    BackgroundedRendererTransition transition) {
  UMA_HISTOGRAM_ENUMERATION("RendererScheduler.BackgroundedRendererTransition",
                            transition, BackgroundedRendererTransition::kCount);
}
}  // namespace scheduler
}  // namespace blink
