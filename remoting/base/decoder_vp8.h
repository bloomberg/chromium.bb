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
  virtual void Initialize(scoped_refptr<media::VideoFrame> frame);
  virtual DecodeResult DecodePacket(const VideoPacket* packet);
  virtual void GetUpdatedRects(UpdatedRects* rects);
  virtual bool IsReadyForData();
  virtual void Reset();
  virtual VideoPacketFormat::Encoding Encoding();
  virtual void SetScaleRatios(double horizontal_ratio, double vertical_ratio);
  virtual void SetClipRect(const gfx::Rect& clip_rect);
  virtual void RefreshRects(const std::vector<gfx::Rect>& rects);

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
  void ConvertRects(const UpdatedRects& rects,
                    UpdatedRects* output_rects);

  // Perform scaling and color space conversion on the specified
  // rectangles.
  // Write the updated rectangles to |output_rects|.
  void ScaleAndConvertRects(const UpdatedRects& rects,
                            UpdatedRects* output_rects);

  // The internal state of the decoder.
  State state_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;

  vpx_codec_ctx_t* codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // Record the updated rects in the last decode.
  UpdatedRects updated_rects_;

  // Clipping rect for the output of the decoder.
  gfx::Rect clip_rect_;

  // Scale factors of the decoded output.
  double horizontal_scale_ratio_;
  double vertical_scale_ratio_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_VP8_H_
