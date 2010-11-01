// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_VERBATIM_H_
#define REMOTING_BASE_ENCODER_VERBATIM_H_

#include "remoting/base/encoder.h"

#include "gfx/rect.h"

namespace remoting {

class UpdateStreamPacket;

// EncoderVerbatim implements Encoder and simply copies input to the output
// buffer verbatim.
class EncoderVerbatim : public Encoder {
 public:
  EncoderVerbatim();
  EncoderVerbatim(int packet_size);

  virtual ~EncoderVerbatim() {}

  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback);

 private:
  // Encode a single dirty rect.
  void EncodeRect(const gfx::Rect& rect, size_t rect_index);

  // Marks a packets as the first in a series of rectangle updates.
  void PrepareUpdateStart(const gfx::Rect& rect, VideoPacket* packet);

  // Retrieves a pointer to the output buffer in |update| used for storing the
  // encoded rectangle data.  Will resize the buffer to |size|.
  uint8* GetOutputBuffer(VideoPacket* packet, size_t size);

  // Submit |message| to |callback_|.
  void SubmitMessage(VideoPacket* packet, size_t rect_index);

  scoped_refptr<CaptureData> capture_data_;
  scoped_ptr<DataAvailableCallback> callback_;
  int packet_size_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_VERBATIM_H_
