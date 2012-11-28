// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include "remoting/base/capture_data.h"
#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcVideoFrameCapturer::IpcVideoFrameCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : delegate_(NULL),
      desktop_session_proxy_(desktop_session_proxy),
      size_most_recent_(SkISize::Make(0, 0)) {
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

media::VideoFrame::Format IpcVideoFrameCapturer::pixel_format() const {
  return media::VideoFrame::RGB32;
}

void IpcVideoFrameCapturer::InvalidateRegion(const SkRegion& invalid_region) {
  desktop_session_proxy_->InvalidateRegion(invalid_region);
}

void IpcVideoFrameCapturer::CaptureFrame() {
  desktop_session_proxy_->CaptureFrame();
}

const SkISize& IpcVideoFrameCapturer::size_most_recent() const {
  return size_most_recent_;
}

void IpcVideoFrameCapturer::OnCaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  if (capture_data)
    size_most_recent_ = capture_data->size();

  if (delegate_)
    delegate_->OnCaptureCompleted(capture_data);
}

}  // namespace remoting
