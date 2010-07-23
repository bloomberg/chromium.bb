// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ENCODER_H_
#define REMOTING_BASE_ENCODER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace media {
  class DataBuffer;
}

namespace remoting {

class CaptureData;
class HostMessage;

// A class to perform the task of encoding a continous stream of
// images.
// This class operates asynchronously to enable maximum throughput.
class Encoder {
 public:

  // EncodingState is a bitfield that tracks the state of the encoding.
  // An encoding that consists of a single block could concievably be starting
  // inprogress and ended at the same time.
  enum {
    EncodingStarting = 1 << 0,
    EncodingInProgress = 1 << 1,
    EncodingEnded = 1 << 2
  };
  typedef int EncodingState;

  // DataAvailableCallback is called as blocks of data are made available
  // from the encoder. Data made available by the encoder is in the form
  // of HostMessage to reduce the amount of memory copies.
  // The callback takes ownership of the HostMessage and is responsible for
  // deleting it.
  typedef Callback2<HostMessage*, EncodingState>::Type DataAvailableCallback;

  virtual ~Encoder() {}

  // Encode an image stored in |capture_data|.
  //
  // If |key_frame| is true, the encoder should not reference
  // previous encode and encode the full frame.
  //
  // When encoded data is available, partial or full |data_available_callback|
  // is called.
  virtual void Encode(scoped_refptr<CaptureData> capture_data,
                      bool key_frame,
                      DataAvailableCallback* data_available_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_ENCODER_H_
