// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcVideoFrameCapturer::IpcVideoFrameCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : delegate_(NULL),
      desktop_session_proxy_(desktop_session_proxy),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

IpcVideoFrameCapturer::~IpcVideoFrameCapturer() {
}

void IpcVideoFrameCapturer::Start(Delegate* delegate) {
  delegate_ = delegate;
  desktop_session_proxy_->SetVideoCapturer(weak_factory_.GetWeakPtr());
}

void IpcVideoFrameCapturer::Stop() {
  delegate_ = NULL;
  weak_factory_.InvalidateWeakPtrs();
}

void IpcVideoFrameCapturer::InvalidateRegion(const SkRegion& invalid_region) {
  desktop_session_proxy_->InvalidateRegion(invalid_region);
}

void IpcVideoFrameCapturer::CaptureFrame() {
  desktop_session_proxy_->CaptureFrame();
}

void IpcVideoFrameCapturer::OnCaptureCompleted(
    scoped_refptr<media::ScreenCaptureData> capture_data) {
  if (delegate_)
    delegate_->OnCaptureCompleted(capture_data);
}

void IpcVideoFrameCapturer::OnCursorShapeChanged(
    scoped_ptr<media::MouseCursorShape> cursor_shape) {
  if (delegate_)
    delegate_->OnCursorShapeChanged(cursor_shape.Pass());
}

}  // namespace remoting
