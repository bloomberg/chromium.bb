// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_metrics_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "public/platform/scheduler/renderer_process_type.h"

namespace blink {
namespace scheduler {

#define TASK_DURATION_METRIC_NAME "RendererScheduler.TaskDurationPerQueueType2"
#define TASK_COUNT_METRIC_NAME "RendererScheduler.TaskCountPerQueueType"
#define MAIN_THREAD_LOAD_METRIC_NAME "RendererScheduler.RendererMainThreadLoad5"
#define EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME \
  MAIN_THREAD_LOAD_METRIC_NAME ".Extension"

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

}  // namespace

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
      task_duration_reporter(TASK_DURATION_METRIC_NAME),
      foreground_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                        ".Foreground"),
      foreground_first_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Foreground.FirstMinute"),
      foreground_second_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Foreground.SecondMinute"),
      foreground_third_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Foreground.ThirdMinute"),
      foreground_after_third_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Foreground.AfterThirdMinute"),
      background_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                        ".Background"),
      background_first_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.FirstMinute"),
      background_second_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.SecondMinute"),
      background_third_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.ThirdMinute"),
      background_fourth_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.FourthMinute"),
      background_fifth_minute_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                                     ".Background.FifthMinute"),
      background_after_fifth_minute_task_duration_reporter(
          TASK_DURATION_METRIC_NAME ".Background.AfterFifthMinute"),
      hidden_task_duration_reporter(TASK_DURATION_METRIC_NAME ".Hidden"),
      visible_task_duration_reporter(TASK_DURATION_METRIC_NAME ".Visible"),
      hidden_music_task_duration_reporter(TASK_DURATION_METRIC_NAME
                                          ".HiddenMusic") {
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

void RendererMetricsHelper::RecordTaskMetrics(
    MainThreadTaskQueue::QueueType queue_type,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
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

  UMA_HISTOGRAM_ENUMERATION(
      TASK_COUNT_METRIC_NAME, static_cast<int>(queue_type),
      static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));

  if (duration >= base::TimeDelta::FromMilliseconds(16)) {
    UMA_HISTOGRAM_ENUMERATION(
        TASK_COUNT_METRIC_NAME ".LongerThan16ms", static_cast<int>(queue_type),
        static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));
  }

  if (duration >= base::TimeDelta::FromMilliseconds(50)) {
    UMA_HISTOGRAM_ENUMERATION(
        TASK_COUNT_METRIC_NAME ".LongerThan50ms", static_cast<int>(queue_type),
        static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));
  }

  if (duration >= base::TimeDelta::FromMilliseconds(100)) {
    UMA_HISTOGRAM_ENUMERATION(
        TASK_COUNT_METRIC_NAME ".LongerThan100ms", static_cast<int>(queue_type),
        static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));
  }

  if (duration >= base::TimeDelta::FromMilliseconds(150)) {
    UMA_HISTOGRAM_ENUMERATION(
        TASK_COUNT_METRIC_NAME ".LongerThan150ms", static_cast<int>(queue_type),
        static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));
  }

  if (duration >= base::TimeDelta::FromSeconds(1)) {
    UMA_HISTOGRAM_ENUMERATION(
        TASK_COUNT_METRIC_NAME ".LongerThan1s", static_cast<int>(queue_type),
        static_cast<int>(MainThreadTaskQueue::QueueType::COUNT));
  }

  task_duration_reporter.RecordTask(queue_type, duration);

  if (renderer_scheduler_->main_thread_only().renderer_backgrounded) {
    background_task_duration_reporter.RecordTask(queue_type, duration);

    // Collect detailed breakdown for first five minutes given that we stop
    // timers on mobile after five minutes.
    base::TimeTicks backgrounded_at =
        renderer_scheduler_->main_thread_only().background_status_changed_at;

    background_first_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time, backgrounded_at,
                        backgrounded_at + base::TimeDelta::FromMinutes(1)));

    background_second_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(1),
                        backgrounded_at + base::TimeDelta::FromMinutes(2)));

    background_third_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(2),
                        backgrounded_at + base::TimeDelta::FromMinutes(3)));

    background_fourth_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(3),
                        backgrounded_at + base::TimeDelta::FromMinutes(4)));

    background_fifth_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        backgrounded_at + base::TimeDelta::FromMinutes(4),
                        backgrounded_at + base::TimeDelta::FromMinutes(5)));

    background_after_fifth_minute_task_duration_reporter.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            start_time, end_time,
            backgrounded_at + base::TimeDelta::FromMinutes(5),
            std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                     end_time)));
  } else {
    foreground_task_duration_reporter.RecordTask(queue_type, duration);

    // For foreground tabs we do not expect such a notable difference as it is
    // the case with background tabs, so we limit breakdown to three minutes.
    base::TimeTicks foregrounded_at =
        renderer_scheduler_->main_thread_only().background_status_changed_at;

    foreground_first_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time, foregrounded_at,
                        foregrounded_at + base::TimeDelta::FromMinutes(1)));

    foreground_second_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        foregrounded_at + base::TimeDelta::FromMinutes(1),
                        foregrounded_at + base::TimeDelta::FromMinutes(2)));

    foreground_third_minute_task_duration_reporter.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        start_time, end_time,
                        foregrounded_at + base::TimeDelta::FromMinutes(2),
                        foregrounded_at + base::TimeDelta::FromMinutes(3)));

    foreground_after_third_minute_task_duration_reporter.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            start_time, end_time,
            foregrounded_at + base::TimeDelta::FromMinutes(3),
            std::max(foregrounded_at + base::TimeDelta::FromMinutes(3),
                     end_time)));
  }

  if (renderer_scheduler_->main_thread_only().renderer_hidden) {
    hidden_task_duration_reporter.RecordTask(queue_type, duration);

    if (renderer_scheduler_->ShouldDisableThrottlingBecauseOfAudio(
            start_time)) {
      hidden_music_task_duration_reporter.RecordTask(queue_type, duration);
    }
  } else {
    visible_task_duration_reporter.RecordTask(queue_type, duration);
  }
}

void RendererMetricsHelper::RecordMainThreadTaskLoad(base::TimeTicks time,
                                                     double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

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

}  // namespace scheduler
}  // namespace blink
