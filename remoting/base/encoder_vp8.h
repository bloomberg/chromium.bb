// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_VP8_H_
#define REMOTING_BASE_ENCODER_VP8_H_

#include <vector>

#include "remoting/base/encoder.h"
#include "ui/gfx/rect.h"

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
  // Initialize the encoder. Returns true if successful.
  bool Init(int width, int height);

  // Prepare |image_| for encoding. Write updated rectangles into
  // |updated_rects|. Returns true if successful.
  bool PrepareImage(scoped_refptr<CaptureData> capture_data,
                    std::vector<gfx::Rect>* updated_rects);

  // Update the active map according to |updated_rects|. Active map is then
  // given to the encoder to speed up encoding.
  void PrepareActiveMap(const std::vector<gfx::Rect>& updated_rects);

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

  // The current frame dimensions.
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(EncoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_VP8_H_
