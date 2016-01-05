// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_
#define REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_

#include <list>

#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class FakeVideoStub : public VideoStub {
 public:
  FakeVideoStub();
  ~FakeVideoStub() override;

  // VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                          const base::Closure& done) override;
 private:
  std::list<scoped_ptr<VideoPacket>> received_packets_;
};

class FakeVideoRenderer : public VideoRenderer {
 public:
  FakeVideoRenderer();
  ~FakeVideoRenderer() override;

  // VideoRenderer interface.
  void OnSessionConfig(const SessionConfig& config) override;
  FakeVideoStub* GetVideoStub() override;
  FrameConsumer* GetFrameConsumer() override;

 private:
  FakeVideoStub video_stub_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_
