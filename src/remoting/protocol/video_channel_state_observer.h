// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
#define REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_

#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver {
 public:
  virtual void OnKeyFrameRequested() = 0;
  virtual void OnTargetBitrateChanged(int bitrate_kbps) = 0;

  // Called when the encoder has finished encoding a frame, and before it is
  // passed to WebRTC's registered callback. |frame| may be null if encoding
  // failed.
  virtual void OnFrameEncoded(
      WebrtcVideoEncoder::EncodeResult encode_result,
      const WebrtcVideoEncoder::EncodedFrame* frame) = 0;

  // Called after the encoded frame is sent via the WebRTC registered callback.
  // The result contains the frame ID assigned by WebRTC if successfully sent.
  // This is only called if the encoder successfully returned a non-null
  // frame. |result| is the result returned by the registered callback.
  virtual void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) = 0;

 protected:
  virtual ~VideoChannelStateObserver() = default;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
