// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_video_renderer.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "remoting/proto/video.pb.h"

namespace remoting {
namespace protocol {

FakeVideoStub::FakeVideoStub() {}
FakeVideoStub::~FakeVideoStub() {}

void FakeVideoStub::ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                                       const base::Closure& done) {
  received_packets_.push_back(std::move(video_packet));
  done.Run();
}

FakeVideoRenderer::FakeVideoRenderer() {}
FakeVideoRenderer::~FakeVideoRenderer() {}

void FakeVideoRenderer::OnSessionConfig(const SessionConfig& config) {}

FakeVideoStub* FakeVideoRenderer::GetVideoStub() {
  return &video_stub_;
}

FrameConsumer* FakeVideoRenderer::GetFrameConsumer() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace protocol
}  // namespace remoting
