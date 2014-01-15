// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/shaped_screen_capturer.h"

#include "base/logging.h"
#include "remoting/host/desktop_shape_tracker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// static
scoped_ptr<ShapedScreenCapturer> ShapedScreenCapturer::Create(
    webrtc::DesktopCaptureOptions options) {
  return scoped_ptr<ShapedScreenCapturer>(
      new ShapedScreenCapturer(scoped_ptr<webrtc::ScreenCapturer>(
                                   webrtc::ScreenCapturer::Create(options)),
                               DesktopShapeTracker::Create(options)));
}

ShapedScreenCapturer::ShapedScreenCapturer(
    scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
    scoped_ptr<DesktopShapeTracker> shape_tracker)
    : screen_capturer_(screen_capturer.Pass()),
      shape_tracker_(shape_tracker.Pass()),
      callback_(NULL) {
}

ShapedScreenCapturer::~ShapedScreenCapturer() {}

void ShapedScreenCapturer::Start(webrtc::ScreenCapturer::Callback* callback) {
  callback_ = callback;
  screen_capturer_->Start(this);
}

void ShapedScreenCapturer::Capture(const webrtc::DesktopRegion& region) {
  screen_capturer_->Capture(region);
}

void ShapedScreenCapturer::SetMouseShapeObserver(
    MouseShapeObserver* mouse_shape_observer) {
  screen_capturer_->SetMouseShapeObserver(mouse_shape_observer);
}

bool ShapedScreenCapturer::GetScreenList(ScreenList* screens) {
  NOTIMPLEMENTED();
  return false;
}

bool ShapedScreenCapturer::SelectScreen(webrtc::ScreenId id) {
  NOTIMPLEMENTED();
  return false;
}

webrtc::SharedMemory* ShapedScreenCapturer::CreateSharedMemory(size_t size) {
  return callback_->CreateSharedMemory(size);
}

void ShapedScreenCapturer::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  shape_tracker_->RefreshDesktopShape();
  frame->set_shape(new webrtc::DesktopRegion(shape_tracker_->desktop_shape()));
  callback_->OnCaptureCompleted(frame);
}

}  // namespace remoting
