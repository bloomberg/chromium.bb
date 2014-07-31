// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_extension_session.h"

#include "remoting/codec/video_encoder.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace remoting {

scoped_ptr<webrtc::ScreenCapturer> HostExtensionSession::OnCreateVideoCapturer(
    scoped_ptr<webrtc::ScreenCapturer> capturer) {
  return capturer.Pass();
}

scoped_ptr<VideoEncoder> HostExtensionSession::OnCreateVideoEncoder(
    scoped_ptr<VideoEncoder> encoder) {
  return encoder.Pass();
}

bool HostExtensionSession::ModifiesVideoPipeline() const {
  return false;
}

} // namespace remoting
