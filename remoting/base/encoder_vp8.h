// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_VP8_H_
#define REMOTING_BASE_ENCODER_VP8_H_

#include "remoting/host/encoder.h"

#include "remoting/base/protocol/chromotocol.pb.h"

extern "C" {
// TODO(garykac): fix this link with the correct path to on2
#include "remoting/third_party/on2/include/on2_encoder.h"
}  // extern "C"

namespace media {

class DataBuffer;

}  // namespace media

namespace remoting {

// A class that uses VP8 to perform encoding.
class EncoderVp8 : public Encoder {
 public:
  EncoderVp8();
  virtual ~EncoderVp8();

  virtual void Encode(const DirtyRects& dirty_rects,
                      const uint8** input_data,
                      const int* strides,
                      bool key_frame,
                      UpdateStreamPacketHeader* header,
                      scoped_refptr<media::DataBuffer>* output_data,
                      bool* encode_done,
                      Task* data_available_task);
  virtual void SetSize(int width, int height);
  virtual void SetPixelFormat(PixelFormat pixel_format);

 private:
  // Setup the VP8 encoder.
  bool Init();

  // True if the encoder is initialized.
  bool initialized_;

  int width_;
  int height_;
  PixelFormat pixel_format_;
  on2_codec_ctx_t codec_;
  on2_image_t image_;
  int last_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(EncoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_VP8_H_
