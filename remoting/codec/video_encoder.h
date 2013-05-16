// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_H_
#define REMOTING_CODEC_VIDEO_ENCODER_H_

#include "base/basictypes.h"
#include "base/callback.h"

class SkRegion;

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoPacket;

// A class to perform the task of encoding a continuous stream of images. The
// interface is asynchronous to enable maximum throughput.
class VideoEncoder {
 public:

  // DataAvailableCallback is called one or more times, for each chunk the
  // underlying video encoder generates.
  typedef base::Callback<void(scoped_ptr<VideoPacket>)> DataAvailableCallback;

  virtual ~VideoEncoder() {}

  // Encode an image stored in |frame|. Doesn't take ownership of |frame|. When
  // encoded data is available, partial or full |data_available_callback| is
  // called.
  virtual void Encode(const webrtc::DesktopFrame* frame,
                      const DataAvailableCallback& data_available_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_H_
