// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_H_
#define REMOTING_CODEC_VIDEO_ENCODER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/data_buffer.h"

namespace media {
class ScreenCaptureData;
}  // namespace media

namespace remoting {

class VideoPacket;

// A class to perform the task of encoding a continous stream of
// images.
// This class operates asynchronously to enable maximum throughput.
class VideoEncoder {
 public:

  // DataAvailableCallback is called one or more times, for each chunk the
  // underlying video encoder generates.
  typedef base::Callback<void(scoped_ptr<VideoPacket>)> DataAvailableCallback;

  virtual ~VideoEncoder() {}

  // Encode an image stored in |capture_data|.
  //
  // If |key_frame| is true, the encoder should not reference
  // previous encode and encode the full frame.
  //
  // When encoded data is available, partial or full |data_available_callback|
  // is called.
  virtual void Encode(scoped_refptr<media::ScreenCaptureData> capture_data,
                      bool key_frame,
                      const DataAvailableCallback& data_available_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_H_
