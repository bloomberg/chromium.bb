// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_activation_client.h"

#include "base/auto_reset.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace aura {

DesktopActivationClient::DesktopActivationClient(RootWindow* root_window)
    : root_window_(root_window),
      current_active_(NULL),
      updating_activation_(false) {
  root_window->AddRootWindowObserver(this);
}

DesktopActivationClient::~DesktopActivationClient() {
  root_window_->RemoveRootWindowObserver(this);
}

void DesktopActivationClient::ActivateWindow(Window* window) {
  // Prevent recursion when called from focus.
  if (updating_activation_)
    return;

  AutoReset<bool> in_activate_window(&updating_activation_, true);
  // Nothing may actually have changed.
  aura::Window* old_active = GetActiveWindow();
  if (old_active == window)
    return;
  // The stacking client may impose rules on what window configurations can be
  // activated or deactivated.
  if (window && !CanActivateWindow(window))
    return;
  // Switch internal focus before we change the activation. Will probably cause
  // recursion.
  if (window &&
      !window->Contains(window->GetFocusManager()->GetFocusedWindow())) {
    window->GetFocusManager()->SetFocusedWindow(window, NULL);
  }

  // Send a deactivation to the old window
  if (current_active_ && aura::client::GetActivationDelegate(current_active_))
    aura::client::GetActivationDelegate(current_active_)->OnLostActive();

  current_active_ = window;

  // Send an activation event to the new window
  if (window && aura::client::GetActivationDelegate(window))
    aura::client::GetActivationDelegate(window)->OnActivated();
}

void DesktopActivationClient::DeactivateWindow(Window* window) {
  if (window == current_active_)
    current_active_ = NULL;
}

aura::Window* DesktopActivationClient::GetActiveWindow() {
  return current_active_;
}

bool DesktopActivationClient::OnWillFocusWindow(Window* window,
                                                const Event* event) {
  return CanActivateWindow(window);
}

void DesktopActivationClient::OnWindowFocused(aura::Window* window) {
  ActivateWindow(GetActivatableWindow(window));
}

bool DesktopActivationClient::CanActivateWindow(aura::Window* window) const {
  return window &&
      window->IsVisible() &&
      (!aura::client::GetActivationDelegate(window) ||
        aura::client::GetActivationDelegate(window)->ShouldActivate(NULL));
}

aura::Window* DesktopActivationClient::GetActivatableWindow(
    aura::Window* window) {
  aura::Window* parent = window->parent();
  aura::Window* child = window;
  while (parent) {
    if (CanActivateWindow(child))
      return child;
    // If |child| isn't activatable, but has transient parent, trace
    // that path instead.
    if (child->transient_parent())
      return GetActivatableWindow(child->transient_parent());
    parent = parent->parent();
    child = child->parent();
  }
  return NULL;
}

}  // namespace aura
