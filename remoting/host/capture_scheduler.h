// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class chooses a capture interval so as to limit CPU usage to not exceed
// a specified %age. It bases this on the CPU usage of recent capture and encode
// operations, and on the number of available CPUs.

#ifndef REMOTING_HOST_CAPTURE_SCHEDULER_H_
#define REMOTING_HOST_CAPTURE_SCHEDULER_H_

#include "base/callback.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/running_average.h"

namespace remoting {

// CaptureScheduler is used by the VideoFramePump to schedule frame capturer,
// taking into account capture delay, encoder delay, network bandwidth, etc.
class CaptureScheduler : public base::NonThreadSafe {
 public:
  // |capture_closure| is called every time a new frame needs to be captured.
  explicit CaptureScheduler(const base::Closure& capture_closure);
  ~CaptureScheduler();

  // Starts the scheduler.
  void Start();

  // Pauses or unpauses the stream.
  void Pause(bool pause);

  // Notifies the scheduler that a capture has been completed.
  void OnCaptureCompleted();

  // Notifies the scheduler that a frame has been encoded.
  void OnFrameEncoded(base::TimeDelta encode_time);

  // Notifies the scheduler that a frame has been sent.
  void OnFrameSent();

  // Sets minimum interval between frames.
  void set_minimum_interval(base::TimeDelta minimum_interval) {
    minimum_interval_ = minimum_interval;
  }

  // Helper functions for tests.
  void SetTickClockForTest(scoped_ptr<base::TickClock> tick_clock);
  void SetTimerForTest(scoped_ptr<base::Timer> timer);
  void SetNumOfProcessorsForTest(int num_of_processors);

 private:
  // Schedules |capture_timer_| to call CaptureNextFrame() at appropriate time.
  // Doesn't do anything if next frame cannot be captured yet (e.g. because
  // there are too many frames being processed).
  void ScheduleNextCapture();

  // Called by |capture_timer_|. Calls |capture_closure_| to start capturing a
  // new frame.
  void CaptureNextFrame();

  base::Closure capture_closure_;

  scoped_ptr<base::TickClock> tick_clock_;

  // Timer used to schedule CaptureNextFrame().
  scoped_ptr<base::Timer> capture_timer_;

  // Minimum interval between frames that determines maximum possible framerate.
  base::TimeDelta minimum_interval_;

  int num_of_processors_;

  RunningAverage capture_time_;
  RunningAverage encode_time_;

  // Total number of pending frames that are being captured, encoded or sent.
  int pending_frames_;

  // Set to true when capture is pending.
  bool capture_pending_;

  // Time at which the last capture started. Used to schedule |capture_timer_|.
  base::TimeTicks last_capture_started_time_;

  bool is_paused_;

  DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURE_SCHEDULER_H_
