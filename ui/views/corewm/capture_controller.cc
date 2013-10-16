// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/capture_controller.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace views {
namespace corewm {

////////////////////////////////////////////////////////////////////////////////
// CaptureController, public:

void CaptureController::Attach(aura::RootWindow* root) {
  DCHECK_EQ(0u, root_windows_.count(root));
  root_windows_.insert(root);
  aura::client::SetCaptureClient(root, this);
}

void CaptureController::Detach(aura::RootWindow* root) {
  root_windows_.erase(root);
  aura::client::SetCaptureClient(root, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// CaptureController, client::CaptureClient implementation:

void CaptureController::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;

  // Make sure window has a root window.
  DCHECK(!new_capture_window || new_capture_window->GetRootWindow());
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;
  aura::RootWindow* old_capture_root = old_capture_window ?
      old_capture_window->GetRootWindow() : NULL;

  // Copy the list in case it's modified out from under us.
  RootWindows root_windows(root_windows_);

  // If we're actually starting capture, then cancel any touches/gestures
  // that aren't already locked to the new window, and transfer any on the
  // old capture window to the new one.  When capture is released we have no
  // distinction between the touches/gestures that were in the window all
  // along (and so shouldn't be canceled) and those that got moved, so
  // just leave them all where they are.
  if (new_capture_window) {
    for (RootWindows::const_iterator i = root_windows.begin();
         i != root_windows.end(); ++i) {
      (*i)->gesture_recognizer()->TransferEventsTo(
          old_capture_window, new_capture_window);
    }
  }

  capture_window_ = new_capture_window;

  for (RootWindows::const_iterator i = root_windows.begin();
       i != root_windows.end(); ++i) {
    (*i)->UpdateCapture(old_capture_window, new_capture_window);
  }

  aura::RootWindow* capture_root =
      capture_window_ ? capture_window_->GetRootWindow() : NULL;
  if (capture_root != old_capture_root) {
    if (old_capture_root)
      old_capture_root->ReleaseNativeCapture();
    if (capture_root)
      capture_root->SetNativeCapture();
  }
}

void CaptureController::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

aura::Window* CaptureController::GetCaptureWindow() {
  return capture_window_;
}

////////////////////////////////////////////////////////////////////////////////
// CaptureController, private:

CaptureController::CaptureController()
    : capture_window_(NULL) {
}

CaptureController::~CaptureController() {
}

////////////////////////////////////////////////////////////////////////////////
// ScopedCaptureClient:

// static
CaptureController* ScopedCaptureClient::capture_controller_ = NULL;

ScopedCaptureClient::ScopedCaptureClient(aura::RootWindow* root)
    : root_window_(root) {
  root->AddObserver(this);
  if (!capture_controller_)
    capture_controller_ = new CaptureController;
  capture_controller_->Attach(root);
}

ScopedCaptureClient::~ScopedCaptureClient() {
  Shutdown();
}

// static
bool ScopedCaptureClient::IsActive() {
  return capture_controller_ && capture_controller_->is_active();
}

void ScopedCaptureClient::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, root_window_);
  Shutdown();
}

void ScopedCaptureClient::Shutdown() {
  if (!root_window_)
    return;

  root_window_->RemoveObserver(this);
  capture_controller_->Detach(root_window_);
  if (!capture_controller_->is_active()) {
    delete capture_controller_;
    capture_controller_ = NULL;
  }
  root_window_ = NULL;
}

}  // namespace corewm
}  // namespace views
