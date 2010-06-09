// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ENCODER_VERBATIM_H_
#define REMOTING_HOST_ENCODER_VERBATIM_H_

#include "remoting/host/encoder.h"

namespace remoting {

// EncoderVerbatim implements Encoder and simply copies input to the output
// buffer verbatim.
class EncoderVerbatim : public Encoder {
 public:
  EncoderVerbatim()
      : width_(0), height_(0), bytes_per_pixel_(0) {}
  virtual ~EncoderVerbatim() {}

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
  // Encode a single dirty rect. Called by Encode().
  // Returns false if there is an error.
  bool EncodeRect(const gfx::Rect& dirty,
                  const uint8** input_data,
                  const int* strides,
                  UpdateStreamPacketHeader* header,
                  scoped_refptr<media::DataBuffer>* output_data);

  int width_;
  int height_;
  int bytes_per_pixel_;
  PixelFormat pixel_format_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ENCODER_VERBATIM_H_
