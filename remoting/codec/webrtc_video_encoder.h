// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_

#include <stdint.h>

#include <memory>

#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

// A class to perform the task of encoding a continuous stream of images. The
// interface is asynchronous to enable maximum throughput.
class WebrtcVideoEncoder {
 public:
  struct FrameParams {
    int bitrate_kbps;
    base::TimeDelta duration;
    bool key_frame;
  };

  struct EncodedFrame {
    webrtc::DesktopSize size;
    std::string data;
    bool key_frame;
    int quantizer;
  };

  virtual ~WebrtcVideoEncoder() {}

  // Encode an image stored in |frame|. If |frame.updated_region()| is empty
  // then the encoder may return a packet (e.g. to top-off previously-encoded
  // portions of the frame to higher quality) or return nullptr to indicate that
  // there is no work to do.
  virtual std::unique_ptr<EncodedFrame> Encode(
      const webrtc::DesktopFrame& frame,
      const FrameParams& param) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_
