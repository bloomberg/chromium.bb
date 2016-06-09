// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_dummy_video_capturer.h"

#include <vector>

namespace remoting {
namespace protocol {

WebrtcDummyVideoCapturer::WebrtcDummyVideoCapturer(
    std::unique_ptr<WebrtcFrameScheduler> frame_scheduler)
    : frame_scheduler_(std::move(frame_scheduler)) {}

WebrtcDummyVideoCapturer::~WebrtcDummyVideoCapturer() {}

cricket::CaptureState WebrtcDummyVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  frame_scheduler_->Start();
  return cricket::CS_RUNNING;
}

void WebrtcDummyVideoCapturer::Stop() {
  frame_scheduler_->Stop();
  SetCaptureState(cricket::CS_STOPPED);
}

bool WebrtcDummyVideoCapturer::IsRunning() {
  return true;
}

bool WebrtcDummyVideoCapturer::IsScreencast() const {
  return true;
}

bool WebrtcDummyVideoCapturer::GetPreferredFourccs(
    std::vector<uint32_t>* fourccs) {
  return true;
}

}  // namespace protocol
}  // namespace remoting
