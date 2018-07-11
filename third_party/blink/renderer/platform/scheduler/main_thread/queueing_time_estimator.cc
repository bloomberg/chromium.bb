// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/queueing_time_estimator.h"

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

#include <algorithm>
#include <map>

namespace blink {
namespace scheduler {

namespace {

#define FRAME_STATUS_PREFIX \
  "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
#define TASK_QUEUE_PREFIX "RendererScheduler.ExpectedQueueingTimeByTaskQueue2."

// On Windows, when a computer sleeps, we may end up getting extremely long
// tasks or idling. We'll ignore tasks longer than |kInvalidPeriodThreshold|.
constexpr base::TimeDelta kInvalidPeriodThreshold =
    base::TimeDelta::FromSecondsD(30);

// This method computes the expected queueing time of a randomly distributed
// task R within a window containing a single task T. Let T' be the time range
// for which T overlaps the window. We first compute the probability that R will
// start within T'. We then compute the expected queueing duration if R does
// start within this range. Since the start time of R is uniformly distributed
// within the window, this is equal to the average of the queueing times if R
// started at the beginning or end of T'. The expected queueing time of T is the
// probability that R will start within T', multiplied by the expected queueing
// duration if R does fall in this range.
base::TimeDelta ExpectedQueueingTimeFromTask(base::TimeTicks task_start,
                                             base::TimeTicks task_end,
                                             base::TimeTicks step_start,
                                             base::TimeTicks step_end) {
  DCHECK_LE(task_start, task_end);
  DCHECK_LE(task_start, step_end);
  DCHECK_LT(step_start, step_end);
  // Because we skip steps when the renderer is backgrounded, we may have gone
  // into the future, and in that case we ignore this task completely.
  if (task_end < step_start)
    return base::TimeDelta();

  base::TimeTicks task_in_step_start_time = std::max(task_start, step_start);
  base::TimeTicks task_in_step_end_time = std::min(task_end, step_end);
  DCHECK_LE(task_in_step_end_time, task_in_step_end_time);

  double probability_of_this_task =
      (task_in_step_end_time - task_in_step_start_time).InMicrosecondsF() /
      (step_end - step_start).InMicrosecondsF();

  base::TimeDelta expected_queueing_duration_within_task =
      ((task_end - task_in_step_start_time) +
       (task_end - task_in_step_end_time)) /
      2;

  return probability_of_this_task * expected_queueing_duration_within_task;
}

}  // namespace

QueueingTimeEstimator::QueueingTimeEstimator(Client* client,
                                             base::TimeDelta window_duration,
                                             int steps_per_window)
    : client_(client),
      window_step_width_(window_duration / steps_per_window),
      renderer_backgrounded_(kLaunchingProcessIsBackgrounded),
      calculator_(steps_per_window) {
  DCHECK_GE(steps_per_window, 1);
}

void QueueingTimeEstimator::OnExecutionStarted(base::TimeTicks now,
                                               MainThreadTaskQueue* queue) {
  DCHECK(!busy_);
  AdvanceTime(now);
  busy_ = true;
  busy_period_start_time_ = now;
  calculator_.UpdateStatusFromTaskQueue(queue);
}

void QueueingTimeEstimator::OnExecutionStopped(base::TimeTicks now) {
  DCHECK(busy_);
  AdvanceTime(now);
  busy_ = false;
  busy_period_start_time_ = base::TimeTicks();
}

void QueueingTimeEstimator::OnRendererStateChanged(
    bool backgrounded,
    base::TimeTicks transition_time) {
  DCHECK_NE(backgrounded, renderer_backgrounded_);
  if (!busy_)
    AdvanceTime(transition_time);
  renderer_backgrounded_ = backgrounded;
}

void QueueingTimeEstimator::AdvanceTime(base::TimeTicks current_time) {
  if (step_start_time_.is_null()) {
    // Ignore any time before the first task.
    if (!busy_)
      return;

    step_start_time_ = busy_period_start_time_;
  }
  base::TimeTicks reference_time =
      busy_ ? busy_period_start_time_ : step_start_time_;
  if (renderer_backgrounded_ ||
      current_time - reference_time > kInvalidPeriodThreshold) {
    // Skip steps when the renderer was backgrounded, when a task took too long,
    // or when we remained idle for too long. May cause |step_start_time_| to go
    // slightly into the future.
    // TODO(npm): crbug.com/776013. Base skipping long tasks/idling on a signal
    // that we've been suspended.
    step_start_time_ =
        current_time.SnappedToNextTick(step_start_time_, window_step_width_);
    calculator_.ResetStep();
    return;
  }
  while (TimePastStepEnd(current_time)) {
    if (busy_) {
      // Include the current task in this window.
      calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
          busy_period_start_time_, current_time, step_start_time_,
          step_start_time_ + window_step_width_));
    }
    calculator_.EndStep(client_);
    step_start_time_ += window_step_width_;
  }
  if (busy_) {
    calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
        busy_period_start_time_, current_time, step_start_time_,
        step_start_time_ + window_step_width_));
  }
}

bool QueueingTimeEstimator::TimePastStepEnd(base::TimeTicks time) {
  return time >= step_start_time_ + window_step_width_;
}

QueueingTimeEstimator::Calculator::Calculator(int steps_per_window)
    : steps_per_window_(steps_per_window),
      step_queueing_times_(steps_per_window) {}

// static
const char* QueueingTimeEstimator::Calculator::GetReportingMessageFromQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kDefault:
      return TASK_QUEUE_PREFIX "Default";
    case MainThreadTaskQueue::QueueType::kUnthrottled:
      return TASK_QUEUE_PREFIX "Unthrottled";
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return TASK_QUEUE_PREFIX "FrameLoading";
    case MainThreadTaskQueue::QueueType::kCompositor:
      return TASK_QUEUE_PREFIX "Compositor";
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return TASK_QUEUE_PREFIX "FrameThrottleable";
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return TASK_QUEUE_PREFIX "FramePausable";
    case MainThreadTaskQueue::QueueType::kControl:
    case MainThreadTaskQueue::QueueType::kIdle:
    case MainThreadTaskQueue::QueueType::kTest:
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
    case MainThreadTaskQueue::QueueType::kV8:
    case MainThreadTaskQueue::QueueType::kOther:
    case MainThreadTaskQueue::QueueType::kCount:
    // Using default here as well because there are some values less than COUNT
    // that have been removed and do not correspond to any QueueType.
    default:
      return TASK_QUEUE_PREFIX "Other";
  }
}

// static
const char*
QueueingTimeEstimator::Calculator::GetReportingMessageFromFrameStatus(
    FrameStatus frame_status) {
  switch (frame_status) {
    case FrameStatus::kMainFrameVisible:
    case FrameStatus::kMainFrameVisibleService:
      return FRAME_STATUS_PREFIX "MainFrameVisible";
    case FrameStatus::kMainFrameHidden:
    case FrameStatus::kMainFrameHiddenService:
      return FRAME_STATUS_PREFIX "MainFrameHidden";
    case FrameStatus::kMainFrameBackground:
    case FrameStatus::kMainFrameBackgroundExemptSelf:
    case FrameStatus::kMainFrameBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "MainFrameBackground";
    case FrameStatus::kSameOriginVisible:
    case FrameStatus::kSameOriginVisibleService:
      return FRAME_STATUS_PREFIX "SameOriginVisible";
    case FrameStatus::kSameOriginHidden:
    case FrameStatus::kSameOriginHiddenService:
      return FRAME_STATUS_PREFIX "SameOriginHidden";
    case FrameStatus::kSameOriginBackground:
    case FrameStatus::kSameOriginBackgroundExemptSelf:
    case FrameStatus::kSameOriginBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "SameOriginBackground";
    case FrameStatus::kCrossOriginVisible:
    case FrameStatus::kCrossOriginVisibleService:
      return FRAME_STATUS_PREFIX "CrossOriginVisible";
    case FrameStatus::kCrossOriginHidden:
    case FrameStatus::kCrossOriginHiddenService:
      return FRAME_STATUS_PREFIX "CrossOriginHidden";
    case FrameStatus::kCrossOriginBackground:
    case FrameStatus::kCrossOriginBackgroundExemptSelf:
    case FrameStatus::kCrossOriginBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "CrossOriginBackground";
    case FrameStatus::kNone:
    case FrameStatus::kDetached:
      return FRAME_STATUS_PREFIX "Other";
    case FrameStatus::kCount:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

void QueueingTimeEstimator::Calculator::UpdateStatusFromTaskQueue(
    MainThreadTaskQueue* queue) {
  current_queue_type_ =
      queue ? queue->queue_type() : MainThreadTaskQueue::QueueType::kOther;
  FrameScheduler* scheduler = queue ? queue->GetFrameScheduler() : nullptr;
  current_frame_status_ =
      scheduler ? GetFrameStatus(scheduler) : FrameStatus::kNone;
}

void QueueingTimeEstimator::Calculator::AddQueueingTime(
    base::TimeDelta queueing_time) {
  step_expected_queueing_time_ += queueing_time;
  eqt_by_queue_type_[static_cast<int>(current_queue_type_)] += queueing_time;
  eqt_by_frame_status_[static_cast<int>(current_frame_status_)] +=
      queueing_time;
}

void QueueingTimeEstimator::Calculator::EndStep(Client* client) {
  step_queueing_times_.Add(step_expected_queueing_time_);

  // MainThreadSchedulerImpl reports the queueing time once per disjoint window.
  //          |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
  // Report:  |-------window EQT------|
  // Discard:         |-------window EQT------|
  // Discard:                 |-------window EQT------|
  // Report:                          |-------window EQT------|
  client->OnQueueingTimeForWindowEstimated(step_queueing_times_.GetAverage(),
                                           step_queueing_times_.IndexIsZero());
  ResetStep();
  if (!step_queueing_times_.IndexIsZero())
    return;

  std::map<const char*, base::TimeDelta> delta_by_message;
  for (int i = 0; i < static_cast<int>(MainThreadTaskQueue::QueueType::kCount);
       ++i) {
    delta_by_message[GetReportingMessageFromQueueType(
        static_cast<MainThreadTaskQueue::QueueType>(i))] +=
        eqt_by_queue_type_[i];
  }
  for (int i = 0; i < static_cast<int>(FrameStatus::kCount); ++i) {
    delta_by_message[GetReportingMessageFromFrameStatus(
        static_cast<FrameStatus>(i))] += eqt_by_frame_status_[i];
  }
  for (auto it : delta_by_message) {
    client->OnReportFineGrainedExpectedQueueingTime(
        it.first, it.second / steps_per_window_);
  }
  std::fill(eqt_by_queue_type_.begin(), eqt_by_queue_type_.end(),
            base::TimeDelta());
  std::fill(eqt_by_frame_status_.begin(), eqt_by_frame_status_.end(),
            base::TimeDelta());
}

void QueueingTimeEstimator::Calculator::ResetStep() {
  step_expected_queueing_time_ = base::TimeDelta();
}

QueueingTimeEstimator::RunningAverage::RunningAverage(int size) {
  circular_buffer_.resize(size);
  index_ = 0;
}

int QueueingTimeEstimator::RunningAverage::GetStepsPerWindow() const {
  return circular_buffer_.size();
}

void QueueingTimeEstimator::RunningAverage::Add(base::TimeDelta bin_value) {
  running_sum_ -= circular_buffer_[index_];
  circular_buffer_[index_] = bin_value;
  running_sum_ += bin_value;
  index_ = (index_ + 1) % circular_buffer_.size();
}

base::TimeDelta QueueingTimeEstimator::RunningAverage::GetAverage() const {
  return running_sum_ / circular_buffer_.size();
}

bool QueueingTimeEstimator::RunningAverage::IndexIsZero() const {
  return index_ == 0;
}

}  // namespace scheduler
}  // namespace blink
