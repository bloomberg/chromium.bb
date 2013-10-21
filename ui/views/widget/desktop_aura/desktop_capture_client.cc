// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace views {

// static
DesktopCaptureClient::Roots* DesktopCaptureClient::roots_ = NULL;

DesktopCaptureClient::DesktopCaptureClient(aura::RootWindow* root)
    : root_(root),
      capture_window_(NULL) {
  if (!roots_)
    roots_ = new Roots;
  roots_->insert(root_);
  aura::client::SetCaptureClient(root, this);
}

DesktopCaptureClient::~DesktopCaptureClient() {
  aura::client::SetCaptureClient(root_, NULL);
  roots_->erase(root_);
}

void DesktopCaptureClient::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;

  // We should only ever be told to capture a child of |root_|. Otherwise
  // things are going to be really confused.
  DCHECK(!new_capture_window ||
         (new_capture_window->GetRootWindow() == root_));
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;

  // Copy the set in case it's modified out from under us.
  Roots roots(*roots_);

  // If we're actually starting capture, then cancel any touches/gestures
  // that aren't already locked to the new window, and transfer any on the
  // old capture window to the new one.  When capture is released we have no
  // distinction between the touches/gestures that were in the window all
  // along (and so shouldn't be canceled) and those that got moved, so
  // just leave them all where they are.
  if (new_capture_window) {
    for (Roots::const_iterator i = roots.begin(); i != roots.end(); ++i) {
      (*i)->gesture_recognizer()->TransferEventsTo(
          old_capture_window, new_capture_window);
    }
  }

  capture_window_ = new_capture_window;

  aura::client::CaptureDelegate* delegate = root_;
  delegate->UpdateCapture(old_capture_window, new_capture_window);

  // Initiate native capture updating.
  if (!capture_window_) {
    delegate->ReleaseNativeCapture();
  } else if (!old_capture_window) {
    delegate->SetNativeCapture();
  }  // else case is capture is remaining in our root, nothing to do.
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
