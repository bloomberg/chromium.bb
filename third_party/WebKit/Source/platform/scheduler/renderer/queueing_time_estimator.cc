// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/queueing_time_estimator.h"

#include "base/memory/ptr_util.h"
#include "platform/scheduler/public/frame_scheduler.h"

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
      static_cast<double>(
          (task_in_step_end_time - task_in_step_start_time).InMicroseconds()) /
      (step_end - step_start).InMicroseconds();

  base::TimeDelta expected_queueing_duration_within_task =
      ((task_end - task_in_step_start_time) +
       (task_end - task_in_step_end_time)) /
      2;

  return base::TimeDelta::FromMillisecondsD(
      probability_of_this_task *
      expected_queueing_duration_within_task.InMillisecondsF());
}

}  // namespace

QueueingTimeEstimator::QueueingTimeEstimator(
    QueueingTimeEstimator::Client* client,
    base::TimeDelta window_duration,
    int steps_per_window)
    : client_(client), state_(steps_per_window) {
  DCHECK_GE(steps_per_window, 1);
  state_.window_step_width = window_duration / steps_per_window;
}

QueueingTimeEstimator::QueueingTimeEstimator(const State& state)
    : client_(nullptr), state_(state) {}

void QueueingTimeEstimator::OnTopLevelTaskStarted(
    base::TimeTicks task_start_time,
    MainThreadTaskQueue* queue) {
  state_.OnTopLevelTaskStarted(client_, task_start_time, queue);
}

void QueueingTimeEstimator::OnTopLevelTaskCompleted(
    base::TimeTicks task_end_time) {
  state_.OnTopLevelTaskCompleted(client_, task_end_time);
}

void QueueingTimeEstimator::OnBeginNestedRunLoop() {
  state_.OnBeginNestedRunLoop();
}

void QueueingTimeEstimator::OnRendererStateChanged(
    bool backgrounded,
    base::TimeTicks transition_time) {
  state_.OnRendererStateChanged(client_, backgrounded, transition_time);
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

void QueueingTimeEstimator::Calculator::EndStep(
    QueueingTimeEstimator::Client* client) {
  step_queueing_times_.Add(step_expected_queueing_time_);

  // RendererScheduler reports the queueing time once per disjoint window.
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

QueueingTimeEstimator::State::State(int steps_per_window)
    : calculator_(steps_per_window) {}

void QueueingTimeEstimator::State::OnTopLevelTaskStarted(
    QueueingTimeEstimator::Client* client,
    base::TimeTicks task_start_time,
    MainThreadTaskQueue* queue) {
  AdvanceTime(client, task_start_time);
  current_task_start_time = task_start_time;
  processing_task = true;
  calculator_.UpdateStatusFromTaskQueue(queue);
}

void QueueingTimeEstimator::State::OnTopLevelTaskCompleted(
    QueueingTimeEstimator::Client* client,
    base::TimeTicks task_end_time) {
  DCHECK(processing_task);
  AdvanceTime(client, task_end_time);
  processing_task = false;
  current_task_start_time = base::TimeTicks();
  in_nested_message_loop_ = false;
}

void QueueingTimeEstimator::State::OnBeginNestedRunLoop() {
  in_nested_message_loop_ = true;
}

void QueueingTimeEstimator::State::OnRendererStateChanged(
    QueueingTimeEstimator::Client* client,
    bool backgrounded,
    base::TimeTicks transition_time) {
  DCHECK_NE(backgrounded, renderer_backgrounded);
  if (!processing_task)
    AdvanceTime(client, transition_time);
  renderer_backgrounded = backgrounded;
}

void QueueingTimeEstimator::State::AdvanceTime(
    QueueingTimeEstimator::Client* client,
    base::TimeTicks current_time) {
  if (step_start_time.is_null()) {
    // Ignore any time before the first task.
    if (!processing_task)
      return;

    step_start_time = current_task_start_time;
  }
  base::TimeTicks reference_time =
      processing_task ? current_task_start_time : step_start_time;
  if (in_nested_message_loop_ || renderer_backgrounded ||
      current_time - reference_time > kInvalidPeriodThreshold) {
    // Skip steps when the renderer was backgrounded, when we are at a nested
    // message loop, when a task took too long, or when we remained idle for
    // too long. May cause |step_start_time| to go slightly into the future.
    // TODO(npm): crbug.com/776013. Base skipping long tasks/idling on a signal
    // that we've been suspended.
    step_start_time =
        current_time.SnappedToNextTick(step_start_time, window_step_width);
    calculator_.ResetStep();
    return;
  }
  while (TimePastStepEnd(current_time)) {
    if (processing_task) {
      // Include the current task in this window.
      calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
          current_task_start_time, current_time, step_start_time,
          step_start_time + window_step_width));
    }
    calculator_.EndStep(client);
    step_start_time += window_step_width;
  }
  if (processing_task) {
    calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
        current_task_start_time, current_time, step_start_time,
        step_start_time + window_step_width));
  }
}

bool QueueingTimeEstimator::State::TimePastStepEnd(base::TimeTicks time) {
  return time >= step_start_time + window_step_width;
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

// Keeps track of the queueing time.
class RecordQueueingTimeClient : public QueueingTimeEstimator::Client {
 public:
  // QueueingTimeEstimator::Client implementation:
  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override {
    queueing_time_ = queueing_time;
  }

  void OnReportFineGrainedExpectedQueueingTime(
      const char* split_description,
      base::TimeDelta queueing_time) override {}

  base::TimeDelta queueing_time() { return queueing_time_; }

  RecordQueueingTimeClient() = default;
  ~RecordQueueingTimeClient() override = default;

 private:
  base::TimeDelta queueing_time_;
  DISALLOW_COPY_AND_ASSIGN(RecordQueueingTimeClient);
};

base::TimeDelta QueueingTimeEstimator::EstimateQueueingTimeIncludingCurrentTask(
    base::TimeTicks now) const {
  RecordQueueingTimeClient record_queueing_time_client;

  // Make a copy of this QueueingTimeEstimator. We'll use it to evaluate the
  // estimated input latency, assuming that any active task ends now.
  QueueingTimeEstimator::State temporary_queueing_time_estimator_state(state_);

  // If there's a task in progress, pretend it ends now, and include it in the
  // computation. If there's no task in progress, add an empty task to flush any
  // stale windows.
  if (temporary_queueing_time_estimator_state.current_task_start_time
          .is_null()) {
    temporary_queueing_time_estimator_state.OnTopLevelTaskStarted(
        &record_queueing_time_client, now, nullptr);
  }
  temporary_queueing_time_estimator_state.OnTopLevelTaskCompleted(
      &record_queueing_time_client, now);

  // Report the max of the queueing time for the last window, or the on-going
  // window (tmp window in chart) which includes the current task.
  //
  //                                Estimate
  //                                  |
  //                                  v
  // Actual Task     |-------------------------...
  // Assumed Task    |----------------|
  // Time       |---o---o---o---o---o---o-------->
  //            0   1   2   3   4   5   6
  //            | s | s | s | s | s | s |
  //            |----last window----|
  //                |----tmp window-----|
  base::TimeDelta last_window_queueing_time =
      record_queueing_time_client.queueing_time();
  temporary_queueing_time_estimator_state.calculator_.EndStep(
      &record_queueing_time_client);
  return std::max(last_window_queueing_time,
                  record_queueing_time_client.queueing_time());
}

}  // namespace scheduler
}  // namespace blink
