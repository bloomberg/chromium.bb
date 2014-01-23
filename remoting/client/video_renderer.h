// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_VIDEO_RENDERER_H_

#include "remoting/protocol/video_stub.h"

namespace remoting {

class ChromotingStats;

namespace protocol {
class SessionConfig;
}  // namespace protocol;

// VideoRenderer is responsible for decoding and displaying incoming video
// stream.
class VideoRenderer : public protocol::VideoStub {
 public:
  // Initializes the processor with the information from the protocol config.
  virtual void Initialize(const protocol::SessionConfig& config) = 0;

  // Return the statistics recorded by this client.
  virtual ChromotingStats* GetStats() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_VIDEO_RENDERER_H_
