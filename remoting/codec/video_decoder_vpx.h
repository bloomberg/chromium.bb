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

typedef struct vpx_image vpx_image_t;

namespace remoting {

class VideoDecoderVpx : public VideoDecoder {
 public:
  // Create decoders for the specified protocol.
  static scoped_ptr<VideoDecoderVpx> CreateForVP8();
  static scoped_ptr<VideoDecoderVpx> CreateForVP9();

  virtual ~VideoDecoderVpx();

  // VideoDecoder interface.
  virtual void Initialize(const webrtc::DesktopSize& screen_size) OVERRIDE;
  virtual bool DecodePacket(const VideoPacket& packet) OVERRIDE;
  virtual void Invalidate(const webrtc::DesktopSize& view_size,
                          const webrtc::DesktopRegion& region) OVERRIDE;
  virtual void RenderFrame(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           webrtc::DesktopRegion* output_region) OVERRIDE;
  virtual const webrtc::DesktopRegion* GetImageShape() OVERRIDE;

 private:
  explicit VideoDecoderVpx(ScopedVpxCodec codec);

  // Calculates the difference between the desktop shape regions in two
  // consecutive frames and updates |updated_region_| and |transparent_region_|
  // accordingly.
  void UpdateImageShapeRegion(webrtc::DesktopRegion* new_desktop_shape);

  ScopedVpxCodec codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // The region updated that hasn't been copied to the screen yet.
  webrtc::DesktopRegion updated_region_;

  // Output dimensions.
  webrtc::DesktopSize screen_size_;

  // The region occupied by the top level windows.
  webrtc::DesktopRegion desktop_shape_;

  // The region that should be make transparent.
  webrtc::DesktopRegion transparent_region_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVpx);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VP8_H_
