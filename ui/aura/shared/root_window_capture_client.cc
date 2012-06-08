// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/root_window_capture_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace aura {
namespace shared {

////////////////////////////////////////////////////////////////////////////////
// RootWindowCaptureClient, public:

RootWindowCaptureClient::RootWindowCaptureClient(RootWindow* root_window)
    : root_window_(root_window),
      capture_window_(NULL) {
  client::SetCaptureClient(root_window, this);
}

RootWindowCaptureClient::~RootWindowCaptureClient() {
  client::SetCaptureClient(root_window_, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowCaptureClient, client::CaptureClient implementation:

void RootWindowCaptureClient::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  root_window_->gesture_recognizer()->TransferEventsTo(capture_window_, window);

  aura::Window* old_capture_window = capture_window_;
  capture_window_ = window;

  if (capture_window_)
    root_window_->SetNativeCapture();
  else
    root_window_->ReleaseNativeCapture();

  root_window_->UpdateCapture(old_capture_window, capture_window_);
}

void RootWindowCaptureClient::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

Window* RootWindowCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

}  // namespace shared
}  // namespace aura
