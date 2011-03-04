// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_ROW_BASED_H_
#define REMOTING_BASE_ENCODER_ROW_BASED_H_

#include "remoting/base/encoder.h"
#include "remoting/proto/video.pb.h"

#include "ui/gfx/rect.h"

namespace remoting {

class Compressor;
class UpdateStreamPacket;

// EncoderRowBased implements an Encoder using zlib or verbatim
// compression. Zlib-based encoder must be created using
// CreateZlibEncoder(), verbatim encoder is created with
// CreateVerbatimEncoder().
//
// Compressor is reset before encoding each rectangle, so that each
// rectangle can be decoded independently.
class EncoderRowBased : public Encoder {
 public:
  static EncoderRowBased* CreateZlibEncoder();
  static EncoderRowBased* CreateZlibEncoder(int packet_size);
  static EncoderRowBased* CreateVerbatimEncoder();
  static EncoderRowBased* CreateVerbatimEncoder(int packet_size);

  virtual ~EncoderRowBased();

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  EncoderRowBased(Compressor* compressor, VideoPacketFormat::Encoding encoding);
  EncoderRowBased(Compressor* compressor, VideoPacketFormat::Encoding encoding,
                  int packet_size);

  // Encode a single dirty rect using compressor.
  void EncodeRect(const gfx::Rect& rect, bool last);

  // Marks a packet as the first in a series of rectangle updates.
  void PrepareUpdateStart(const gfx::Rect& rect, VideoPacket* packet);

  // Retrieves a pointer to the output buffer in |update| used for storing the
  // encoded rectangle data.  Will resize the buffer to |size|.
  uint8* GetOutputBuffer(VideoPacket* packet, size_t size);

  // Submit |message| to |callback_|.
  void SubmitMessage(VideoPacket* packet, size_t rect_index);

  // The encoding of the incoming stream.
  VideoPacketFormat::Encoding encoding_;

  scoped_ptr<Compressor> compressor_;

  scoped_refptr<CaptureData> capture_data_;
  scoped_ptr<DataAvailableCallback> callback_;

  // The most recent screen size.
  int screen_width_;
  int screen_height_;

  int packet_size_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_ROW_BASED_H_
