// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_activation_client.h"

#include "ui/aura/window.h"

namespace aura {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, public:

TestActivationClient::TestActivationClient() : active_window_(NULL) {
  ActivationClient::SetActivationClient(this);
}

TestActivationClient::~TestActivationClient() {
}

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, ActivationClient implementation:

void TestActivationClient::ActivateWindow(Window* window) {
  if (active_window_)
    active_window_->RemoveObserver(this);
  active_window_ = window;
  active_window_->AddObserver(this);
}

void TestActivationClient::DeactivateWindow(Window* window) {
  if (window == active_window_) {
    if (active_window_)
      active_window_->RemoveObserver(this);
    active_window_ = NULL;
  }
}

Window* TestActivationClient::GetActiveWindow() {
  return active_window_;
}

bool TestActivationClient::CanFocusWindow(Window* window) const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, WindowObserver implementation:

void TestActivationClient::OnWindowDestroyed(Window* window) {
  if (window == active_window_) {
    window->RemoveObserver(this);
    active_window_ = NULL;
  }
}

}  // namespace test
}  // namespace aura
