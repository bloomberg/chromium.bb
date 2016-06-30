// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_video_renderer.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

FakeVideoStub::FakeVideoStub() {}
FakeVideoStub::~FakeVideoStub() {}

void FakeVideoStub::set_on_frame_callback(base::Closure on_frame_callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  on_frame_callback_ = on_frame_callback;
}

void FakeVideoStub::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    const base::Closure& done) {
  CHECK(thread_checker_.CalledOnValidThread());
  received_packets_.push_back(std::move(video_packet));
  if (!done.is_null())
    done.Run();
  if (!on_frame_callback_.is_null())
    on_frame_callback_.Run();
}

FakeFrameConsumer::FakeFrameConsumer() {}
FakeFrameConsumer::~FakeFrameConsumer() {}

void FakeFrameConsumer::set_on_frame_callback(base::Closure on_frame_callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  on_frame_callback_ = on_frame_callback;
}

std::unique_ptr<webrtc::DesktopFrame> FakeFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  CHECK(thread_checker_.CalledOnValidThread());
  return base::WrapUnique(new webrtc::BasicDesktopFrame(size));
}

void FakeFrameConsumer::DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                  const base::Closure& done) {
  CHECK(thread_checker_.CalledOnValidThread());
  received_frames_.push_back(std::move(frame));
  if (!done.is_null())
    done.Run();
  if (!on_frame_callback_.is_null())
    on_frame_callback_.Run();
}

FrameConsumer::PixelFormat FakeFrameConsumer::GetPixelFormat() {
  CHECK(thread_checker_.CalledOnValidThread());
  return FORMAT_BGRA;
}

FakeVideoRenderer::FakeVideoRenderer() {}
FakeVideoRenderer::~FakeVideoRenderer() {}

bool FakeVideoRenderer::Initialize(const ClientContext& client_context,
                                   protocol::PerformanceTracker* perf_tracker) {
  return true;
}

void FakeVideoRenderer::OnSessionConfig(const SessionConfig& config) {}

FakeVideoStub* FakeVideoRenderer::GetVideoStub() {
  CHECK(thread_checker_.CalledOnValidThread());
  return &video_stub_;
}

FakeFrameConsumer* FakeVideoRenderer::GetFrameConsumer() {
  CHECK(thread_checker_.CalledOnValidThread());
  return &frame_consumer_;
}

}  // namespace protocol
}  // namespace remoting
