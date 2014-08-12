// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/shaped_desktop_capturer.h"

#include "base/logging.h"
#include "remoting/host/desktop_shape_tracker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

ShapedDesktopCapturer::ShapedDesktopCapturer(
    scoped_ptr<webrtc::DesktopCapturer> desktop_capturer,
    scoped_ptr<DesktopShapeTracker> shape_tracker)
    : desktop_capturer_(desktop_capturer.Pass()),
      shape_tracker_(shape_tracker.Pass()),
      callback_(NULL) {
}

ShapedDesktopCapturer::~ShapedDesktopCapturer() {}

void ShapedDesktopCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
  callback_ = callback;
  desktop_capturer_->Start(this);
}

void ShapedDesktopCapturer::Capture(const webrtc::DesktopRegion& region) {
  desktop_capturer_->Capture(region);
}

webrtc::SharedMemory* ShapedDesktopCapturer::CreateSharedMemory(size_t size) {
  return callback_->CreateSharedMemory(size);
}

void ShapedDesktopCapturer::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  shape_tracker_->RefreshDesktopShape();
  frame->set_shape(new webrtc::DesktopRegion(shape_tracker_->desktop_shape()));
  callback_->OnCaptureCompleted(frame);
}

}  // namespace remoting
