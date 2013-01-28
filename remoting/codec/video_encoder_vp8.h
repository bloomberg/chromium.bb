// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_VP8_H_
#define REMOTING_CODEC_VIDEO_ENCODER_VP8_H_

#include "base/gtest_prod_util.h"
#include "remoting/codec/video_encoder.h"
#include "third_party/skia/include/core/SkRegion.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

// A class that uses VP8 to perform encoding.
class VideoEncoderVp8 : public VideoEncoder {
 public:
  VideoEncoderVp8();
  virtual ~VideoEncoderVp8();

  virtual void Encode(
      scoped_refptr<media::ScreenCaptureData> capture_data,
      bool key_frame,
      const DataAvailableCallback& data_available_callback) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(VideoEncoderVp8Test, AlignAndClipRect);

  // Initialize the encoder. Returns true if successful.
  bool Init(const SkISize& size);

  // Destroy the encoder.
  void Destroy();

  // Prepare |image_| for encoding. Write updated rectangles into
  // |updated_region|.
  void PrepareImage(scoped_refptr<media::ScreenCaptureData> capture_data,
                    SkRegion* updated_region);

  // Update the active map according to |updated_region|. Active map is then
  // given to the encoder to speed up encoding.
  void PrepareActiveMap(const SkRegion& updated_region);

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

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_VP8_H_
