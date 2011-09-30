// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_VP8_H_
#define REMOTING_BASE_ENCODER_VP8_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "remoting/base/encoder.h"
#include "third_party/skia/include/core/SkRect.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

// A class that uses VP8 to perform encoding.
class EncoderVp8 : public Encoder {
 public:
  EncoderVp8();
  virtual ~EncoderVp8();

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  typedef std::vector<SkIRect> RectVector;

  FRIEND_TEST_ALL_PREFIXES(EncoderVp8Test, AlignAndClipRect);

  // Initialize the encoder. Returns true if successful.
  bool Init(const SkISize& size);

  // Destroy the encoder.
  void Destroy();

  // Prepare |image_| for encoding. Write updated rectangles into
  // |updated_rects|. Returns true if successful.
  bool PrepareImage(scoped_refptr<CaptureData> capture_data,
                    RectVector* updated_rects);

  // Update the active map according to |updated_rects|. Active map is then
  // given to the encoder to speed up encoding.
  void PrepareActiveMap(const RectVector& updated_rects);

  // Align the sides of the rectangle to multiples of 2 (expanding outwards),
  // but ensuring the result stays within the screen area (width, height).
  // If |rect| falls outside the screen area, return an empty rect.
  //
  // TODO(lambroslambrou): Pull this out if it's useful for other things than
  // VP8-encoding?
  static SkIRect AlignAndClipRect(const SkIRect& rect, int width, int height);

  // True if the encoder is initialized.
  bool initialized_;

  scoped_ptr<vpx_codec_ctx_t> codec_;
  scoped_ptr<vpx_image_t> image_;
  scoped_array<uint8> active_map_;
  int active_map_width_;
  int active_map_height_;
  int last_timestamp_;

  // Buffer for storing the yuv image.
  scoped_array<uint8> yuv_image_;

  // The current frame size.
  SkISize size_;

  DISALLOW_COPY_AND_ASSIGN(EncoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_VP8_H_
