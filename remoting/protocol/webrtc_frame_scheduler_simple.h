// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/timer/timer.h"
#include "remoting/base/leaky_bucket.h"
#include "remoting/base/running_samples.h"

namespace remoting {
namespace protocol {

// WebrtcFrameSchedulerSimple is a simple implementation of
// WebrtcFrameScheduler that always keeps only one frame in the pipeline.
// It schedules each frame after the previous one is expected to finish sending.
class WebrtcFrameSchedulerSimple : public WebrtcFrameScheduler {
 public:
  WebrtcFrameSchedulerSimple();
  ~WebrtcFrameSchedulerSimple() override;

  // WebrtcFrameScheduler implementation.
  void Start(const base::Closure& capture_callback) override;
  void Pause(bool pause) override;
  void SetKeyFrameRequest() override;
  void SetTargetBitrate(int bitrate_kbps) override;
  bool GetEncoderFrameParams(
      const webrtc::DesktopFrame& frame,
      WebrtcVideoEncoder::FrameParams* params_out) override;
  void OnFrameEncoded(
      const WebrtcVideoEncoder::EncodedFrame& encoded_frame,
      const webrtc::EncodedImageCallback::Result& send_result) override;

 private:
  void ScheduleNextFrame(base::TimeTicks now);
  void CaptureNextFrame();

  base::Closure capture_callback_;
  bool paused_ = false;
  bool key_frame_request_ = false;

  base::TimeTicks last_capture_started_time_;

  LeakyBucket pacing_bucket_;

  // Set to true when encoding unchanged frames for top-off.
  bool top_off_is_active_ = false;

  // Accumulator for capture and encoder delay history.
  RunningSamples frame_processing_delay_us_;

  // Accumulator for updated region area in the previously encoded frames.
  RunningSamples updated_region_area_;

  base::OneShotTimer capture_timer_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_
