// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_capture_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace views {

DesktopCaptureClient::DesktopCaptureClient() : capture_window_(NULL) {
}

DesktopCaptureClient::~DesktopCaptureClient() {
}

void DesktopCaptureClient::SetCapture(aura::Window* window) {
  if (window) {
    DCHECK(window->GetRootWindow());
    if (capture_window_)
      DCHECK_EQ(window->GetRootWindow(), capture_window_->GetRootWindow());
  }

  aura::Window* old_capture = capture_window_;
  capture_window_ = window;

  aura::RootWindow* root_window = window ? window->GetRootWindow() :
      capture_window_ ? capture_window_->GetRootWindow() : NULL;
  if (root_window)
    root_window->UpdateCapture(old_capture, window);
}

void DesktopCaptureClient::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

aura::Window* DesktopCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

}  // namespace views