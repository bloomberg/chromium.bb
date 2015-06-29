// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_video_renderer.h"

#include "base/logging.h"
#include "remoting/proto/video.pb.h"

namespace remoting {
namespace test {

TestVideoRenderer::TestVideoRenderer() : video_frames_processed_(0) {
}

TestVideoRenderer::~TestVideoRenderer() {
}

void TestVideoRenderer::OnSessionConfig(const protocol::SessionConfig& config) {
  DVLOG(2) << "TestVideoRenderer::OnSessionConfig() Called";
}

ChromotingStats* TestVideoRenderer::GetStats() {
  DVLOG(2) << "TestVideoRenderer::GetStats() Called";
  return nullptr;
}

protocol::VideoStub* TestVideoRenderer::GetVideoStub() {
  DVLOG(2) << "TestVideoRenderer::GetVideoStub() Called";
  return this;
}

void TestVideoRenderer::ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                                           const base::Closure& done) {
  if (!video_packet->data().empty()) {
    // If the video frame was not a keep alive frame (i.e. not empty) then
    // count it.
    DVLOG(2) << "Video Packet Processed, Total: " << ++video_frames_processed_;
  } else {
    // Log at a high verbosity level as we receive empty packets frequently and
    // they can clutter up the debug output if the level is set too low.
    DVLOG(3) << "Empty Video Packet received.";
  }

  done.Run();
}

}  // namespace test
}  // namespace remoting
