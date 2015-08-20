// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_H_
#define REMOTING_CODEC_VIDEO_DECODER_H_

#include "base/basictypes.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoPacket;

// Interface for a decoder that decodes video packets.
class VideoDecoder {
 public:
  VideoDecoder() {}
  virtual ~VideoDecoder() {}

  // Decodes a video frame. Returns false in case of a failure. The caller must
  // pre-allocate a |frame| with the size specified in the |packet|.
  virtual bool DecodePacket(const VideoPacket& packet,
                            webrtc::DesktopFrame* frame) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_H_
