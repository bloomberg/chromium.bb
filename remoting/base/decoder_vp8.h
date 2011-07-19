// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_VP8_H_
#define REMOTING_BASE_DECODER_VP8_H_

#include "remoting/base/decoder.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;

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

 private:
  enum State {
    kUninitialized,
    kReady,
    kError,
  };

  // The internal state of the decoder.
  State state_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;

  vpx_codec_ctx_t* codec_;

  // Record the updated rects in the last decode.
  UpdatedRects updated_rects_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_VP8_H_
