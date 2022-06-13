// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"

namespace remoting {
namespace protocol {

// WebrtcFrameSchedulerConstantRate is an implementation of WebrtcFrameScheduler
// that captures frames at a fixed rate. It uses the maximum frame rate provided
// by SetMaxFramerateFps().
class WebrtcFrameSchedulerConstantRate : public WebrtcFrameScheduler {
 public:
  WebrtcFrameSchedulerConstantRate();

  WebrtcFrameSchedulerConstantRate(const WebrtcFrameSchedulerConstantRate&) =
      delete;
  WebrtcFrameSchedulerConstantRate& operator=(
      const WebrtcFrameSchedulerConstantRate&) = delete;

  ~WebrtcFrameSchedulerConstantRate() override;

  // VideoChannelStateObserver implementation.
  void OnKeyFrameRequested() override;
  void OnTargetBitrateChanged(int bitrate_kbps) override;
  void OnFrameEncoded(
      WebrtcVideoEncoder::EncodeResult encode_result,
      const WebrtcVideoEncoder::EncodedFrame* encoded_frame) override;
  void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) override;

  // WebrtcFrameScheduler implementation.
  void Start(const base::RepeatingClosure& capture_callback) override;
  void Pause(bool pause) override;
  void OnFrameCaptured(const webrtc::DesktopFrame* frame) override;
  void SetMaxFramerateFps(int max_framerate_fps) override;

 private:
  void ScheduleNextFrame();
  void CaptureNextFrame();

  base::RepeatingClosure capture_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool paused_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::OneShotTimer capture_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeTicks last_capture_started_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set to true when a frame is being captured. Used to avoid scheduling more
  // than one capture in parallel.
  bool frame_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Framerate for scheduling frames. Initially 0 to prevent scheduling before
  // the output sink has been added.
  int max_framerate_fps_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_
