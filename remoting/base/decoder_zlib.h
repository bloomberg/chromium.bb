// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_ZLIB_H_
#define REMOTING_BASE_DECODER_ZLIB_H_

#include "remoting/base/decoder.h"

namespace remoting {

class DecompressorZlib;

class DecoderZlib : public Decoder {
 public:
  DecoderZlib();

  // Decoder implementations.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame,
                           UpdatedRects* update_rects,
                           Task* partial_decode_done,
                           Task* decode_done);
  virtual bool PartialDecode(ChromotingHostMessage* message);
  virtual void EndDecode();

  // TODO(hclam): Should make this into the Decoder interface.
  void set_reverse_rows(bool reverse) { reverse_rows_ = reverse; }

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
  int bytes_per_pixel_;

  // Tasks to call when decode is done.
  scoped_ptr<Task> partial_decode_done_;
  scoped_ptr<Task> decode_done_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* updated_rects_;

  // A zlib decompressor used throughout one update sequence.
  scoped_ptr<DecompressorZlib> decompressor_;

  // The position in the row that we are updating.
  int row_pos_;

  // The row number in the rect that we are updaing.
  int row_y_;

  // True if we should decode the image upside down.
  bool reverse_rows_;

  DISALLOW_COPY_AND_ASSIGN(DecoderZlib);
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_ZLIB_H_
