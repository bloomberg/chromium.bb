// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  ~DecoderVp8();

  // Decoder implementations.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame,
                           UpdatedRects* update_rects,
                           Task* partial_decode_done,
                           Task* decode_done);
  virtual bool PartialDecode(ChromotingHostMessage* message);
  virtual void EndDecode();

 private:
  bool HandleBeginRect(ChromotingHostMessage* message);
  bool HandleRectData(ChromotingHostMessage* message);
  bool HandleEndRect(ChromotingHostMessage* message);

  // The internal state of the decoder.
  State state_;

  // Keeps track of the updating rect.
  int rect_x_;
  int rect_y_;
  int rect_width_;
  int rect_height_;

  // Tasks to call when decode is done.
  scoped_ptr<Task> partial_decode_done_;
  scoped_ptr<Task> decode_done_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* updated_rects_;

  vpx_codec_ctx_t* codec_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_VP8_H_
