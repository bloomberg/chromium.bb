// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DECODER_VERBATIM_H_
#define REMOTING_CLIENT_DECODER_VERBATIM_H_

#include "remoting/client/decoder.h"

namespace remoting {

class DecoderVerbatim : public Decoder {
 public:
  DecoderVerbatim(PartialDecodeDoneCallback* partial_decode_done_callback,
                  DecodeDoneCallback* decode_done_callback)
      : Decoder(partial_decode_done_callback, decode_done_callback) {
  }

  // Decoder implementations.
  virtual bool BeginDecode(scoped_refptr<media::VideoFrame> frame);
  virtual bool PartialDecode(chromotocol_pb::HostMessage* message);
  virtual void EndDecode();

 private:
  // The video frame to write to.
  scoped_refptr<media::VideoFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(DecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DECODER_VERBATIM_H_
