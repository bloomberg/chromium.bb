// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_VIDEO_RENDERER_H_
#define REMOTING_TEST_TEST_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/client/video_renderer.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace test {

// Processes video packets as they are received from the remote host.
class TestVideoRenderer : public VideoRenderer, public protocol::VideoStub {
 public:
  TestVideoRenderer();
  ~TestVideoRenderer() override;

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  ChromotingStats* GetStats() override;
  protocol::VideoStub* GetVideoStub() override;

 private:
  // protocol::VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                          const base::Closure& done) override;

  // Track the number of populated video frames which have been received.
  int video_frames_processed_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoRenderer);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_TEST_VIDEO_RENDERER_H_
