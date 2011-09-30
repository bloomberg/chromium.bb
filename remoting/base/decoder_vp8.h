// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  virtual void GetUpdatedRects(RectVector* rects) OVERRIDE;
  virtual bool IsReadyForData() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual VideoPacketFormat::Encoding Encoding() OVERRIDE;
  virtual void SetScaleRatios(double horizontal_ratio,
                              double vertical_ratio) OVERRIDE;
  virtual void SetClipRect(const SkIRect& clip_rect) OVERRIDE;
  virtual void RefreshRects(const RectVector& rects) OVERRIDE;

 private:
  enum State {
    kUninitialized,
    kReady,
    kError,
  };

  // Return true if scaling is enabled
  bool DoScaling() const;

  // Perform color space conversion on the specified rectangles.
  // Write the updated rectangles to |output_rects|.
  void ConvertRects(const RectVector& rects,
                    RectVector* output_rects);

  // Perform scaling and color space conversion on the specified
  // rectangles.
  // Write the updated rectangles to |output_rects|.
  void ScaleAndConvertRects(const RectVector& rects,
                            RectVector* output_rects);

  // The internal state of the decoder.
  State state_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;

  vpx_codec_ctx_t* codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // Record the updated rects in the last decode.
  RectVector updated_rects_;

  // Clipping rect for the output of the decoder.
  SkIRect clip_rect_;

  // Scale factors of the decoded output.
  double horizontal_scale_ratio_;
  double vertical_scale_ratio_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_VP8_H_
