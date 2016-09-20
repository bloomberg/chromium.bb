// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

#include "base/callback_forward.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/video_encoder.h"

namespace remoting {
namespace protocol {

// An abstract interface for frame schedulers, which are responsible for
// scheduling when video frames are captured and for defining encoding
// parameters for each frame.
class WebrtcFrameScheduler {
 public:
  WebrtcFrameScheduler() {}
  virtual ~WebrtcFrameScheduler() {}

  // Starts the scheduler. |capture_callback| will be called whenever a new
  // frame should be captured.
  virtual void Start(const base::Closure& capture_callback) = 0;

  // Pause and resumes the scheduler.
  virtual void Pause(bool pause) = 0;

  // Requests a key frame.
  virtual void SetKeyFrameRequest() = 0;

  // Sets network bitrate estimate.
  virtual void SetTargetBitrate(int bitrate_kbps) = 0;

  // Called after |frame| has been captured to get encoding parameters for the
  // frame. Returns false if the frame should be dropped (e.g. when there are
  // no changed), true otherwise.
  virtual bool GetEncoderFrameParams(
      const webrtc::DesktopFrame& frame,
      WebrtcVideoEncoder::FrameParams* params_out) = 0;

  // Called after a frame has been encoded and passed to the sender.
  virtual void OnFrameEncoded(
      const WebrtcVideoEncoder::EncodedFrame& encoded_frame,
      const webrtc::EncodedImageCallback::Result& send_result) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
