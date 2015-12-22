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
}  // namespace webrtc

namespace remoting {
namespace protocol {

class VideoStream {
 public:
  // Callback used to notify about screen size changes.
  typedef base::Callback<void(const webrtc::DesktopSize& size)> SizeCallback;

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

  // Sets SizeCallback to be called when screen size is changed.
  virtual void SetSizeCallback(const SizeCallback& size_callback) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_STREAM_H_
