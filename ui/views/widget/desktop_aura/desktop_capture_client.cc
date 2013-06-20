// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "ui/aura/root_window.h"

namespace views {

std::set<DesktopCaptureClient*> DesktopCaptureClient::live_capture_clients_;

DesktopCaptureClient::DesktopCaptureClient(aura::RootWindow* root_window)
    : root_window_(root_window),
      capture_window_(NULL) {
  aura::client::SetCaptureClient(root_window_, this);
  live_capture_clients_.insert(this);
}

DesktopCaptureClient::~DesktopCaptureClient() {
  live_capture_clients_.erase(this);
  aura::client::SetCaptureClient(root_window_, NULL);
}

void DesktopCaptureClient::SetCapture(aura::Window* window) {
  if (capture_window_ == window)
    return;
  if (window) {
    // If we're actually starting capture, then cancel any touches/gestures
    // that aren't already locked to the new window, and transfer any on the
    // old capture window to the new one.  When capture is released we have no
    // distinction between the touches/gestures that were in the window all
    // along (and so shouldn't be canceled) and those that got moved, so
    // just leave them all where they are.
    for (std::set<DesktopCaptureClient*>::iterator it =
             live_capture_clients_.begin(); it != live_capture_clients_.end();
         ++it) {
      (*it)->root_window_->gesture_recognizer()->TransferEventsTo(
          capture_window_, window);
    }
  }
  aura::Window* old_capture_window = GetCaptureWindow();
  capture_window_ = window;

  if (capture_window_) {
    root_window_->SetNativeCapture();

    for (std::set<DesktopCaptureClient*>::iterator it =
             live_capture_clients_.begin(); it != live_capture_clients_.end();
         ++it) {
      if (*it != this)
        (*it)->OnOtherCaptureClientTookCapture();
    }
  } else {
    root_window_->ReleaseNativeCapture();
  }

  root_window_->UpdateCapture(old_capture_window, capture_window_);
}

void DesktopCaptureClient::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

aura::Window* DesktopCaptureClient::GetCaptureWindow() {
  for (std::set<DesktopCaptureClient*>::iterator it =
            live_capture_clients_.begin(); it != live_capture_clients_.end();
        ++it) {
    if ((*it)->capture_window_)
      return (*it)->capture_window_;
  }
  return NULL;
}

void DesktopCaptureClient::OnOtherCaptureClientTookCapture() {
  if (capture_window_ == NULL) {
    // While RootWindow may not technically have capture, it will store state
    // that needs to be cleared on capture changed regarding mouse up/down.
    root_window_->ClearMouseHandlers();
  }
  else {
    SetCapture(NULL);
  }
}

}  // namespace views
