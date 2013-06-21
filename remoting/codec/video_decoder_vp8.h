// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VP8_H_
#define REMOTING_CODEC_VIDEO_DECODER_VP8_H_

#include "base/compiler_specific.h"
#include "remoting/codec/video_decoder.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

class VideoDecoderVp8 : public VideoDecoder {
 public:
  VideoDecoderVp8();
  virtual ~VideoDecoderVp8();

  // VideoDecoder implementations.
  virtual void Initialize(const SkISize& screen_size) OVERRIDE;
  virtual DecodeResult DecodePacket(const VideoPacket* packet) OVERRIDE;
  virtual bool IsReadyForData() OVERRIDE;
  virtual VideoPacketFormat::Encoding Encoding() OVERRIDE;
  virtual void Invalidate(const SkISize& view_size,
                          const SkRegion& region) OVERRIDE;
  virtual void RenderFrame(const SkISize& view_size,
                           const SkIRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           SkRegion* output_region) OVERRIDE;
  virtual const SkRegion* GetImageShape() OVERRIDE;

 private:
  enum State {
    kUninitialized,
    kReady,
    kError,
  };

  // Fills the rectangle |rect| with the given ARGB color |color| in |buffer|.
  void FillRect(uint8* buffer, int stride, const SkIRect& rect, uint32 color);

  // Calculates the difference between the desktop shape regions in two
  // consecutive frames and updates |updated_region_| and |transparent_region_|
  // accordingly.
  void UpdateImageShapeRegion(SkRegion* new_desktop_shape);

  // The internal state of the decoder.
  State state_;

  vpx_codec_ctx_t* codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // The region updated that hasn't been copied to the screen yet.
  SkRegion updated_region_;

  // Output dimensions.
  SkISize screen_size_;

  // The region occupied by the top level windows.
  SkRegion desktop_shape_;

  // The region that should be make transparent.
  SkRegion transparent_region_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VP8_H_
