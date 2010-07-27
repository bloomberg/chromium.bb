// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_ZLIB_H_
#define REMOTING_BASE_ENCODER_ZLIB_H_

#include "remoting/base/encoder.h"

namespace remoting {

class CompressorZlib;
class UpdateStreamPacket;

// EncoderZlib implements an Encoder using Zlib for compression.
class EncoderZlib : public Encoder {
 public:
  EncoderZlib();
  EncoderZlib(int packet_size);

  virtual ~EncoderZlib() {}

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  // Encode a single dirty rect using compressor.
  void EncodeRect(CompressorZlib* compressor);

  // Create a new HostMessage with the right flag and attributes. The message
  // can be used immediately for output of encoding.
  HostMessage* PrepareMessage(bool new_rect);

  // Submit |message| to |callback_|.
  void SubmitMessage(HostMessage* message);

  scoped_refptr<CaptureData> capture_data_;
  scoped_ptr<DataAvailableCallback> callback_;
  size_t current_rect_;
  int packet_size_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_ZLIB_H_
