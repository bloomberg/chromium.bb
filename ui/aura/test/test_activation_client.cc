// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_activation_client.h"

#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace aura {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, public:

TestActivationClient::TestActivationClient(RootWindow* root_window) {
  client::SetActivationClient(root_window, this);
}

TestActivationClient::~TestActivationClient() {
  for (unsigned int i = 0; i < active_windows_.size(); ++i) {
    active_windows_[i]->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, client::ActivationClient implementation:

void TestActivationClient::AddObserver(
    client::ActivationChangeObserver* observer) {
}

void TestActivationClient::RemoveObserver(
    client::ActivationChangeObserver* observer) {
}

void TestActivationClient::ActivateWindow(Window* window) {
  Window *last_active = GetActiveWindow();
  if (last_active == window)
    return;

  RemoveActiveWindow(window);
  active_windows_.push_back(window);
  window->AddObserver(this);
  if (aura::client::GetActivationDelegate(window))
    aura::client::GetActivationDelegate(window)->OnActivated();

  if (last_active && aura::client::GetActivationDelegate(last_active))
    aura::client::GetActivationDelegate(last_active)->OnLostActive();
}

void TestActivationClient::DeactivateWindow(Window* window) {
  if (aura::client::GetActivationDelegate(window))
    aura::client::GetActivationDelegate(window)->OnLostActive();
}

Window* TestActivationClient::GetActiveWindow() {
  if (active_windows_.empty())
    return NULL;
  return active_windows_.back();
}

bool TestActivationClient::OnWillFocusWindow(Window* window,
                                             const Event* event) {
  return true;
}

bool TestActivationClient::CanActivateWindow(Window* window) const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TestActivationClient, WindowObserver implementation:

void TestActivationClient::OnWindowDestroyed(Window* window) {
  if (window == GetActiveWindow()) {
    window->RemoveObserver(this);
    active_windows_.pop_back();
    Window* next_active = GetActiveWindow();
    if (next_active && aura::client::GetActivationDelegate(next_active))
      aura::client::GetActivationDelegate(next_active)->OnActivated();
    return;
  }

  RemoveActiveWindow(window);
}

void TestActivationClient::RemoveActiveWindow(Window* window) {
  for (unsigned int i = 0; i < active_windows_.size(); ++i) {
    if (active_windows_[i] == window) {
      active_windows_.erase(active_windows_.begin() + i);
      window->RemoveObserver(this);
      return;
    }
  }
}

}  // namespace test
}  // namespace aura
