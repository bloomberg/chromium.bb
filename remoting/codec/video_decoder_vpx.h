// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VPX_H_
#define REMOTING_CODEC_VIDEO_DECODER_VPX_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_decoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

typedef const struct vpx_codec_iface vpx_codec_iface_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

class VideoDecoderVpx : public VideoDecoder {
 public:
  // Create decoders for the specified protocol.
  static scoped_ptr<VideoDecoderVpx> CreateForVP8();
  static scoped_ptr<VideoDecoderVpx> CreateForVP9();

  ~VideoDecoderVpx() override;

  // VideoDecoder interface.
  bool DecodePacket(const VideoPacket& packet) override;
  void Invalidate(const webrtc::DesktopSize& view_size,
                  const webrtc::DesktopRegion& region) override;
  void RenderFrame(const webrtc::DesktopSize& view_size,
                   const webrtc::DesktopRect& clip_area,
                   uint8* image_buffer,
                   int image_stride,
                   webrtc::DesktopRegion* output_region) override;
  const webrtc::DesktopRegion* GetImageShape() override;

 private:
  explicit VideoDecoderVpx(vpx_codec_iface_t* codec);

  // Returns the dimensions of the most recent frame as a DesktopSize.
  webrtc::DesktopSize image_size() const;

  ScopedVpxCodec codec_;

  // Pointer to the most recently decoded image.
  vpx_image_t* image_;

  // Area of the source that has changed since the last RenderFrame call.
  webrtc::DesktopRegion updated_region_;

  // The shape of the most-recent frame, if any.
  scoped_ptr<webrtc::DesktopRegion> desktop_shape_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVpx);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VP8_H_
