// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_VERBATIM_H_
#define REMOTING_BASE_ENCODER_VERBATIM_H_

#include "remoting/base/encoder.h"

namespace remoting {

class UpdateStreamPacket;

// EncoderVerbatim implements Encoder and simply copies input to the output
// buffer verbatim.
class EncoderVerbatim : public Encoder {
 public:
  EncoderVerbatim() {}
  virtual ~EncoderVerbatim() {}

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  // Encode a single dirty rect. Called by Encode(). Output is written
  // to |msg|.
  // Returns false if there is an error.
  bool EncodeRect(int x, int y, int width, int height,
                  const scoped_refptr<CaptureData>& capture_data,
                  UpdateStreamPacketMessage* msg);
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_VERBATIM_H_
