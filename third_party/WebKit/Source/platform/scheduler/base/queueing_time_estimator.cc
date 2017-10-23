// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/queueing_time_estimator.h"

#include "base/memory/ptr_util.h"

#include <algorithm>

namespace blink {
namespace scheduler {

namespace {

// On Windows, when a computer sleeps, we may end up getting an extremely long
// task. We'll ignore tasks longer than |invalidTaskThreshold|.
constexpr base::TimeDelta kInvalidTaskThreshold =
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
  state_.window_duration = window_duration;
  state_.window_step_width = window_duration / steps_per_window;
}

QueueingTimeEstimator::QueueingTimeEstimator(const State& state)
    : client_(nullptr), state_(state) {}

void QueueingTimeEstimator::OnTopLevelTaskStarted(
    base::TimeTicks task_start_time) {
  state_.OnTopLevelTaskStarted(client_, task_start_time);
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

QueueingTimeEstimator::State::State(int steps_per_window)
    : step_queueing_times(steps_per_window) {}

void QueueingTimeEstimator::State::OnTopLevelTaskStarted(
    QueueingTimeEstimator::Client* client,
    base::TimeTicks task_start_time) {
  AdvanceTime(client, task_start_time);
  current_task_start_time = task_start_time;
  processing_task = true;
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
  if (in_nested_message_loop_ || renderer_backgrounded) {
    // Skip steps when the renderer was backgrounded or when we are at a nested
    // message loop. May cause |step_start_time| to go slightly into the future.
    step_start_time =
        current_time.SnappedToNextTick(step_start_time, window_step_width);
    step_expected_queueing_time = base::TimeDelta();
    return;
  }
  if (processing_task &&
      current_time - current_task_start_time > kInvalidTaskThreshold) {
    // This task took too long, so we'll pretend it never happened. This could
    // be because the user's machine went to sleep during a task.
    return;
  }
  while (TimePastStepEnd(current_time)) {
    if (processing_task) {
      // Include the current task in this window.
      step_expected_queueing_time += ExpectedQueueingTimeFromTask(
          current_task_start_time, current_time, step_start_time,
          step_start_time + window_step_width);
    }
    step_queueing_times.Add(step_expected_queueing_time);

    // RendererScheduler reports the queueing time once per disjoint window.
    //          |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
    // Report:  |-------window EQT------|
    // Discard:         |-------window EQT------|
    // Discard:                 |-------window EQT------|
    // Report:                          |-------window EQT------|
    client->OnQueueingTimeForWindowEstimated(step_queueing_times.GetAverage(),
                                             step_queueing_times.IndexIsZero());
    step_start_time += window_step_width;
    step_expected_queueing_time = base::TimeDelta();
  }
  if (processing_task) {
    step_expected_queueing_time += ExpectedQueueingTimeFromTask(
        current_task_start_time, current_time, step_start_time,
        step_start_time + window_step_width);
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

  base::TimeDelta queueing_time() { return queueing_time_; }

  RecordQueueingTimeClient() {}
  ~RecordQueueingTimeClient() override {}

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
        &record_queueing_time_client, now);
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
  RunningAverage& temporary_step_queueing_times =
      temporary_queueing_time_estimator_state.step_queueing_times;
  temporary_step_queueing_times.Add(
      temporary_queueing_time_estimator_state.step_expected_queueing_time);

  return std::max(record_queueing_time_client.queueing_time(),
                  temporary_step_queueing_times.GetAverage());
}

}  // namespace scheduler
}  // namespace blink
