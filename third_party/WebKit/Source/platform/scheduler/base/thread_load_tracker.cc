// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/thread_load_tracker.h"

#include <algorithm>

namespace blink {
namespace scheduler {

namespace {

const int kLoadReportingIntervalInSeconds = 1;
const int kWaitingPeriodBeforeReportingInSeconds = 10;

}  // namespace

ThreadLoadTracker::ThreadLoadTracker(base::TimeTicks now,
                                     const Callback& callback)
    : time_(now),
      next_reporting_time_(now),
      thread_state_(ThreadState::ACTIVE),
      last_state_change_time_(now),
      waiting_period_(
          base::TimeDelta::FromSeconds(kWaitingPeriodBeforeReportingInSeconds)),
      reporting_interval_(
          base::TimeDelta::FromSeconds(kLoadReportingIntervalInSeconds)),
      callback_(callback) {}

ThreadLoadTracker::~ThreadLoadTracker() {}

void ThreadLoadTracker::Pause(base::TimeTicks now) {
  Advance(now, TaskState::IDLE);
  thread_state_ = ThreadState::PAUSED;
  last_state_change_time_ = now;
}

void ThreadLoadTracker::Resume(base::TimeTicks now) {
  Advance(now, TaskState::IDLE);
  thread_state_ = ThreadState::ACTIVE;
  last_state_change_time_ = now;
}

void ThreadLoadTracker::RecordTaskTime(base::TimeTicks start_time,
                                       base::TimeTicks end_time) {
  start_time = std::max(last_state_change_time_, start_time);
  end_time = std::max(last_state_change_time_, end_time);

  Advance(start_time, TaskState::IDLE);
  Advance(end_time, TaskState::TASK_RUNNING);
}

void ThreadLoadTracker::RecordIdle(base::TimeTicks now) {
  Advance(now, TaskState::IDLE);
}

void ThreadLoadTracker::Advance(base::TimeTicks now, TaskState task_state) {
  // This function advances |time_| to now and calls |callback_|
  // when appropriate.
  if (time_ > now) {
    return;
  }

  if (thread_state_ == ThreadState::PAUSED) {
    // If the load tracker is paused, bail out early.
    time_ = now;
    next_reporting_time_ = now + reporting_interval_;
    return;
  }

  while (time_ < now) {
    // Forward time_ to the earliest of following:
    // a) time to call |callback_|
    // b) requested time to forward (|now|).
    base::TimeTicks next_current_time = std::min(next_reporting_time_, now);

    base::TimeDelta delta = next_current_time - time_;

    // Forward time and recalculate |total_time_| and |total_runtime_|.
    if (thread_state_ == ThreadState::ACTIVE) {
      total_time_ += delta;
      if (task_state == TaskState::TASK_RUNNING) {
        total_runtime_ += delta;
      }
    }
    time_ = next_current_time;

    if (time_ == next_reporting_time_) {
      // Call |callback_| if need and update next callback time.
      if (thread_state_ == ThreadState::ACTIVE &&
          total_time_ >= waiting_period_) {
        callback_.Run(time_, Load());
      }
      next_reporting_time_ += reporting_interval_;
    }
  }
}

double ThreadLoadTracker::Load() {
  if (total_time_.is_zero()) {
    return 0;
  }
  return total_runtime_.InSecondsF() / total_time_.InSecondsF();
}

}  // namespace scheduler
}  // namespace blink
