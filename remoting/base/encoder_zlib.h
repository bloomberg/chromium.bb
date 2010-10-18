// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_ZLIB_H_
#define REMOTING_BASE_ENCODER_ZLIB_H_

#include "remoting/base/encoder.h"

#include "gfx/rect.h"

namespace remoting {

class CompressorZlib;
class UpdateStreamPacket;

// EncoderZlib implements an Encoder using Zlib for compression.
class EncoderZlib : public Encoder {
 public:
  EncoderZlib();
  EncoderZlib(int packet_size);

  virtual ~EncoderZlib();

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  // Encode a single dirty rect using compressor.
  void EncodeRect(CompressorZlib* compressor, const gfx::Rect& rect,
                  size_t rect_index);

  // Marks a packets as the first in a series of rectangle updates.
  void PrepareUpdateStart(const gfx::Rect& rect,
                          RectangleUpdatePacket* update);

  // Retrieves a pointer to the output buffer in |update| used for storing the
  // encoded rectangle data.  Will resize the buffer to |size|.
  uint8* GetOutputBuffer(RectangleUpdatePacket* update, size_t size);

  // Submit |message| to |callback_|.
  void SubmitMessage(ChromotingHostMessage* message, size_t rect_index);

  scoped_refptr<CaptureData> capture_data_;
  scoped_ptr<DataAvailableCallback> callback_;
  //size_t current_rect_;
  int packet_size_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_ZLIB_H_
