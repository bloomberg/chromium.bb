// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_STUB_H_

#include "base/callback_forward.h"

namespace remoting {

class VideoPacket;

namespace protocol {

class VideoStub {
 public:
  virtual ~VideoStub() { }

  // TODO(sergeyu): VideoPacket is the protobuf message that is used to send
  // video packets in protobuf stream. It should not be used here. Add another
  // struct and use it to represent video packets internally.
  virtual void ProcessVideoPacket(const VideoPacket* video_packet,
                                  const base::Closure& done) = 0;

  // Returns number of packets currently pending in the buffer.
  virtual int GetPendingPackets() = 0;

 protected:
  VideoStub() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_STUB_H_
