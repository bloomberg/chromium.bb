// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_VIDEO_STREAM_H_

#include <stdint.h>

#include <cstdint>

#include "base/callback_forward.h"

namespace webrtc {
class DesktopSize;
class DesktopVector;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class VideoStream {
 public:
  class Observer {
   public:
    // Called to notify about screen size changes. The size is specified in
    // physical pixels.
    virtual void OnVideoSizeChanged(VideoStream* stream,
                                    const webrtc::DesktopSize& size,
                                    const webrtc::DesktopVector& dpi) = 0;

    // Called to notify about an outgoing video frame. |input_event_timestamp|
    // corresponds to the last input event that was injected before the frame
    // was captured.
    virtual void OnVideoFrameSent(VideoStream* stream,
                                  uint32_t frame_id,
                                  int64_t event_timestamp) = 0;
  };

  VideoStream() {}
  virtual ~VideoStream() {}

  // Pauses or resumes scheduling of frame captures. Pausing/resuming captures
  // only affects capture scheduling and does not stop/start the capturer.
  virtual void Pause(bool pause) = 0;

  // Should be called whenever an input event is received.
  virtual void OnInputEventReceived(int64_t event_timestamp) = 0;

  // Sets whether the video encoder should be requested to encode losslessly,
  // or to use a lossless color space (typically requiring higher bandwidth).
  virtual void SetLosslessEncode(bool want_lossless) = 0;
  virtual void SetLosslessColor(bool want_lossless) = 0;

  // Sets stream observer.
  virtual void SetObserver(Observer* observer) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_STREAM_H_
