// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capture_scheduler.h"

#include <algorithm>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"

namespace {

// Number of samples to average the most recent capture and encode time
// over.
const int kStatisticsWindow = 3;

// The hard limit is 30fps or 33ms per recording cycle.
const int64 kDefaultMinimumIntervalMs = 33;

// Controls how much CPU time we can use for encode and capture.
// Range of this value is between 0 to 1. 0 means using 0% of of all CPUs
// available while 1 means using 100% of all CPUs available.
const double kRecordingCpuConsumption = 0.5;

// Maximum number of frames that can be processed simultaneously.
static const int kMaxPendingFrames = 2;

}  // namespace

namespace remoting {

// We assume that the number of available cores is constant.
CaptureScheduler::CaptureScheduler(const base::Closure& capture_closure)
    : capture_closure_(capture_closure),
      tick_clock_(new base::DefaultTickClock()),
      capture_timer_(new base::Timer(false, false)),
      minimum_interval_(
          base::TimeDelta::FromMilliseconds(kDefaultMinimumIntervalMs)),
      num_of_processors_(base::SysInfo::NumberOfProcessors()),
      capture_time_(kStatisticsWindow),
      encode_time_(kStatisticsWindow),
      pending_frames_(0),
      capture_pending_(false),
      is_paused_(false) {
  DCHECK(num_of_processors_);
}

CaptureScheduler::~CaptureScheduler() {
}

void CaptureScheduler::Start() {
  DCHECK(CalledOnValidThread());

  ScheduleNextCapture();
}

void CaptureScheduler::Pause(bool pause) {
  DCHECK(CalledOnValidThread());

  if (is_paused_ != pause) {
    is_paused_ = pause;

    if (is_paused_) {
      capture_timer_->Stop();
    } else {
      ScheduleNextCapture();
    }
  }
}

void CaptureScheduler::OnCaptureCompleted() {
  DCHECK(CalledOnValidThread());

  capture_pending_ = false;
  capture_time_.Record(
      (tick_clock_->NowTicks() - last_capture_started_time_).InMilliseconds());

  ScheduleNextCapture();
}

void CaptureScheduler::OnFrameSent() {
  DCHECK(CalledOnValidThread());

  // Decrement the pending capture count.
  pending_frames_--;
  DCHECK_GE(pending_frames_, 0);

  ScheduleNextCapture();
}

void CaptureScheduler::OnFrameEncoded(base::TimeDelta encode_time) {
  DCHECK(CalledOnValidThread());

  encode_time_.Record(encode_time.InMilliseconds());
  ScheduleNextCapture();
}

void CaptureScheduler::SetTickClockForTest(
    scoped_ptr<base::TickClock> tick_clock) {
  tick_clock_ = tick_clock.Pass();
}
void CaptureScheduler::SetTimerForTest(scoped_ptr<base::Timer> timer) {
  capture_timer_ = timer.Pass();
}
void CaptureScheduler::SetNumOfProcessorsForTest(int num_of_processors) {
  num_of_processors_ = num_of_processors;
}

void CaptureScheduler::ScheduleNextCapture() {
  DCHECK(CalledOnValidThread());

  if (is_paused_ || pending_frames_ >= kMaxPendingFrames || capture_pending_)
    return;

  // Delay by an amount chosen such that if capture and encode times
  // continue to follow the averages, then we'll consume the target
  // fraction of CPU across all cores.
  base::TimeDelta delay =
      std::max(minimum_interval_,
               base::TimeDelta::FromMilliseconds(
                   (capture_time_.Average() + encode_time_.Average()) /
                   (kRecordingCpuConsumption * num_of_processors_)));

  // Account for the time that has passed since the last capture.
  delay = std::max(base::TimeDelta(), delay - (tick_clock_->NowTicks() -
                                               last_capture_started_time_));

  capture_timer_->Start(
      FROM_HERE, delay,
      base::Bind(&CaptureScheduler::CaptureNextFrame, base::Unretained(this)));
}

void CaptureScheduler::CaptureNextFrame() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_paused_);
  DCHECK(!capture_pending_);

  pending_frames_++;
  DCHECK_LE(pending_frames_, kMaxPendingFrames);

  capture_pending_ = true;
  last_capture_started_time_ = tick_clock_->NowTicks();
  capture_closure_.Run();
}

}  // namespace remoting
