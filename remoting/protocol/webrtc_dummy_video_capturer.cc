// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_dummy_video_capturer.h"

#include <vector>

namespace remoting {
namespace protocol {

WebRtcDummyVideoCapturer::WebRtcDummyVideoCapturer(
    std::unique_ptr<WebRtcFrameScheduler> frame_scheduler)
    : frame_scheduler_(std::move(frame_scheduler)) {}

WebRtcDummyVideoCapturer::~WebRtcDummyVideoCapturer() {}

cricket::CaptureState WebRtcDummyVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  frame_scheduler_->Start();
  return cricket::CS_RUNNING;
}

void WebRtcDummyVideoCapturer::Stop() {
  frame_scheduler_->Stop();
  SetCaptureState(cricket::CS_STOPPED);
}

bool WebRtcDummyVideoCapturer::IsRunning() {
  return true;
}

bool WebRtcDummyVideoCapturer::IsScreencast() const {
  return true;
}

bool WebRtcDummyVideoCapturer::GetPreferredFourccs(
    std::vector<uint32_t>* fourccs) {
  return true;
}

}  // namespace protocol
}  // namespace remoting
