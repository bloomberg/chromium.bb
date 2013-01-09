// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include "remoting/capturer/capture_data.h"
#include "remoting/capturer/mouse_cursor_shape.h"
#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcVideoFrameCapturer::IpcVideoFrameCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : delegate_(NULL),
      desktop_session_proxy_(desktop_session_proxy) {
}

IpcVideoFrameCapturer::~IpcVideoFrameCapturer() {
}

void IpcVideoFrameCapturer::Start(Delegate* delegate) {
  delegate_ = delegate;
  desktop_session_proxy_->StartVideoCapturer(this);
}

void IpcVideoFrameCapturer::Stop() {
  desktop_session_proxy_->StopVideoCapturer();
  delegate_ = NULL;
}

void IpcVideoFrameCapturer::InvalidateRegion(const SkRegion& invalid_region) {
  desktop_session_proxy_->InvalidateRegion(invalid_region);
}

void IpcVideoFrameCapturer::CaptureFrame() {
  desktop_session_proxy_->CaptureFrame();
}

void IpcVideoFrameCapturer::OnCaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  if (delegate_)
    delegate_->OnCaptureCompleted(capture_data);
}

void IpcVideoFrameCapturer::OnCursorShapeChanged(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  if (delegate_)
    delegate_->OnCursorShapeChanged(cursor_shape.Pass());
}

}  // namespace remoting
