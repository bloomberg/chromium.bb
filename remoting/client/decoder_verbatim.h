// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DECODER_VERBATIM_H_
#define REMOTING_CLIENT_DECODER_VERBATIM_H_

#include "remoting/client/decoder.h"

namespace remoting {

class DecoderVerbatim : public Decoder {
 public:
  DecoderVerbatim();

  // Decoder implementations.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame,
                           UpdatedRects* update_rects,
                           Task* partial_decode_done,
                           Task* decode_done);
  virtual bool PartialDecode(HostMessage* message);
  virtual void EndDecode();

 private:
  // Tasks to call when decode is done.
  scoped_ptr<Task> partial_decode_done_;
  scoped_ptr<Task> decode_done_;

  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* updated_rects_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DECODER_VERBATIM_H_
