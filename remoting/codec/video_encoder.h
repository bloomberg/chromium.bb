// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_H_
#define REMOTING_CODEC_VIDEO_ENCODER_H_

#include "base/memory/scoped_ptr.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoPacket;

// A class to perform the task of encoding a continuous stream of images. The
// interface is asynchronous to enable maximum throughput.
class VideoEncoder {
 public:
  virtual ~VideoEncoder() {}

  // Request that the encoder provide lossless encoding, or color, if possible.
  virtual void SetLosslessEncode(bool want_lossless) {}
  virtual void SetLosslessColor(bool want_lossless) {}

  // Encode an image stored in |frame|.
  virtual scoped_ptr<VideoPacket> Encode(const webrtc::DesktopFrame& frame) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_H_
