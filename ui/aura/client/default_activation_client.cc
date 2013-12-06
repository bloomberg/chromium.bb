// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/default_activation_client.h"

#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/window.h"

namespace aura {
namespace client {

////////////////////////////////////////////////////////////////////////////////
// DefaultActivationClient, public:

DefaultActivationClient::DefaultActivationClient(Window* root_window)
    : last_active_(NULL) {
  client::SetActivationClient(root_window, this);
}

DefaultActivationClient::~DefaultActivationClient() {
  for (unsigned int i = 0; i < active_windows_.size(); ++i) {
    active_windows_[i]->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DefaultActivationClient, client::ActivationClient implementation:

void DefaultActivationClient::AddObserver(
    client::ActivationChangeObserver* observer) {
  observers_.AddObserver(observer);
}

void DefaultActivationClient::RemoveObserver(
    client::ActivationChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DefaultActivationClient::ActivateWindow(Window* window) {
  Window* last_active = GetActiveWindow();
  if (last_active == window)
    return;

  last_active_ = last_active;
  RemoveActiveWindow(window);
  active_windows_.push_back(window);
  window->parent()->StackChildAtTop(window);
  window->AddObserver(this);

  FOR_EACH_OBSERVER(client::ActivationChangeObserver,
                    observers_,
                    OnWindowActivated(window, last_active));

  aura::client::ActivationChangeObserver* observer =
      aura::client::GetActivationChangeObserver(last_active);
  if (observer)
    observer->OnWindowActivated(window, last_active);
  observer = aura::client::GetActivationChangeObserver(window);
  if (observer)
    observer->OnWindowActivated(window, last_active);
}

void DefaultActivationClient::DeactivateWindow(Window* window) {
  aura::client::ActivationChangeObserver* observer =
      aura::client::GetActivationChangeObserver(window);
  if (observer)
    observer->OnWindowActivated(NULL, window);
  if (last_active_)
    ActivateWindow(last_active_);
}

Window* DefaultActivationClient::GetActiveWindow() {
  if (active_windows_.empty())
    return NULL;
  return active_windows_.back();
}

Window* DefaultActivationClient::GetActivatableWindow(Window* window) {
  return NULL;
}

Window* DefaultActivationClient::GetToplevelWindow(Window* window) {
  return NULL;
}

bool DefaultActivationClient::OnWillFocusWindow(Window* window,
                                                const ui::Event* event) {
  return true;
}

bool DefaultActivationClient::CanActivateWindow(Window* window) const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DefaultActivationClient, WindowObserver implementation:

void DefaultActivationClient::OnWindowDestroyed(Window* window) {
  if (window == last_active_)
    last_active_ = NULL;

  if (window == GetActiveWindow()) {
    active_windows_.pop_back();
    Window* next_active = GetActiveWindow();
    if (next_active && aura::client::GetActivationChangeObserver(next_active)) {
      aura::client::GetActivationChangeObserver(next_active)->OnWindowActivated(
          next_active, NULL);
    }
    return;
  }

  RemoveActiveWindow(window);
}

void DefaultActivationClient::RemoveActiveWindow(Window* window) {
  for (unsigned int i = 0; i < active_windows_.size(); ++i) {
    if (active_windows_[i] == window) {
      active_windows_.erase(active_windows_.begin() + i);
      window->RemoveObserver(this);
      return;
    }
  }
}

}  // namespace client
}  // namespace aura
