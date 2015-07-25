// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURE_SCHEDULER_H_
#define REMOTING_HOST_CAPTURE_SCHEDULER_H_

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/running_average.h"
#include "remoting/protocol/video_feedback_stub.h"

namespace remoting {

class VideoPacket;

// CaptureScheduler is used by the VideoFramePump to schedule frame capturer,
// taking into account capture delay, encoder delay, network bandwidth, etc.
// It implements VideoFeedbackStub to receive frame acknowledgments from the
// client.
//
// It attempts to achieve the following goals when scheduling frames:
//  - Keep round-trip latency as low a possible.
//  - Parallelize capture, encode and transmission, to achieve frame rate as
//    close to the target of 30fps as possible.
//  - Limit CPU usage to 50%.
class CaptureScheduler : public protocol::VideoFeedbackStub {
 public:
  explicit CaptureScheduler(const base::Closure& capture_closure);
  ~CaptureScheduler() override;

  // Starts the scheduler.
  void Start();

  // Pauses or unpauses the stream.
  void Pause(bool pause);

  // Notifies the scheduler that a capture has been completed.
  void OnCaptureCompleted();

  // Notifies the scheduler that a frame has been encoded. The scheduler can
  // change |packet| if necessary, e.g. set |frame_id|.
  void OnFrameEncoded(VideoPacket* packet);

  // Notifies the scheduler that a frame has been sent.
  void OnFrameSent();

  // VideoFeedbackStub interface.
  void ProcessVideoAck(scoped_ptr<VideoAck> video_ack) override;

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

  // Number of frames pending encoding.
  int num_encoding_frames_;

  // Number of outgoing frames for which we haven't received an acknowledgment.
  int num_unacknowledged_frames_;

  // Set to true when capture is pending.
  bool capture_pending_;

  // Time at which the last capture started. Used to schedule |capture_timer_|.
  base::TimeTicks last_capture_started_time_;

  bool is_paused_;

  // Frame ID to be assigned to the next outgoing video frame.
  uint32_t next_frame_id_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURE_SCHEDULER_H_
