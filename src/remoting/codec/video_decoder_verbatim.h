// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_
#define REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/codec/video_decoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// Video decoder implementations that decodes video packet encoded by
// VideoEncoderVerbatim. It just copies data from incoming packets to the
// video frames.
class VideoDecoderVerbatim : public VideoDecoder {
 public:
  VideoDecoderVerbatim();
  ~VideoDecoderVerbatim() override;

  // VideoDecoder implementation.
  void SetPixelFormat(PixelFormat pixel_format) override;
  bool DecodePacket(const VideoPacket& packet,
                    webrtc::DesktopFrame* frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_
