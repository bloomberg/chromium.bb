// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_VP8_H_
#define REMOTING_BASE_DECODER_VP8_H_

#include "remoting/base/decoder.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

class DecoderVp8 : public Decoder {
 public:
  DecoderVp8();
  virtual ~DecoderVp8();

  // Decoder implementations.
  virtual void Initialize(scoped_refptr<media::VideoFrame> frame) OVERRIDE;
  virtual DecodeResult DecodePacket(const VideoPacket* packet) OVERRIDE;
  virtual void GetUpdatedRegion(SkRegion* region) OVERRIDE;
  virtual bool IsReadyForData() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual VideoPacketFormat::Encoding Encoding() OVERRIDE;
  virtual void SetOutputSize(const SkISize& size) OVERRIDE;
  virtual void SetClipRect(const SkIRect& clip_rect) OVERRIDE;
  virtual void RefreshRegion(const SkRegion& region) OVERRIDE;

 private:
  enum State {
    kUninitialized,
    kReady,
    kError,
  };

  // Return true if scaling is enabled
  bool DoScaling() const;

  // Perform color space conversion on the specified region.
  // Writes the updated region to |output_region|.
  void ConvertRegion(const SkRegion& region,
                     SkRegion* output_region);

  // Perform scaling and color space conversion on the specified
  // region.  Writes the updated rectangles to |output_region|.
  void ScaleAndConvertRegion(const SkRegion& region,
                             SkRegion* output_region);

  // The internal state of the decoder.
  State state_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;

  vpx_codec_ctx_t* codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // The region updated by the most recent decode.
  SkRegion updated_region_;

  // Clipping rect for the output of the decoder.
  SkIRect clip_rect_;

  // Output dimensions.
  SkISize output_size_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_VP8_H_
