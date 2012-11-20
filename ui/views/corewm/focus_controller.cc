// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_controller.h"

#include "ui/views/corewm/focus_change_event.h"

namespace views {
namespace corewm {

////////////////////////////////////////////////////////////////////////////////
// FocusController, public:

FocusController::FocusController()
    : active_window_(NULL),
      focused_window_(NULL) {
  FocusChangeEvent::RegisterEventTypes();
}

FocusController::~FocusController() {
}

void FocusController::SetFocusedWindow(aura::Window* focused_window) {
  // TODO(beng): rules.
  focused_window_ = focused_window;
}

// TODO(beng): CanFocusWindow(). Implement rules.

////////////////////////////////////////////////////////////////////////////////
// FocusController, aura::client::ActivationClient implementation:

void FocusController::AddObserver(
    aura::client::ActivationChangeObserver* observer) {
  NOTREACHED();
}

void FocusController::RemoveObserver(
    aura::client::ActivationChangeObserver* observer) {
  NOTREACHED();
}

void FocusController::ActivateWindow(aura::Window* window) {
// TODO(beng):
}

void FocusController::DeactivateWindow(aura::Window* window) {
// TODO(beng):
}

aura::Window* FocusController::GetActiveWindow() {
  // TODO(beng):
  return NULL;
}

bool FocusController::OnWillFocusWindow(aura::Window* window,
                                        const ui::Event* event) {
  NOTREACHED();
  return false;
}

bool FocusController::CanActivateWindow(aura::Window* window) const {
  // TODO(beng): implement activation rules.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventHandler implementation:
ui::EventResult FocusController::OnKeyEvent(ui::KeyEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(beng): GetFocusableWindow().
  SetFocusedWindow(static_cast<aura::Window*>(event->target()));
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnTouchEvent(ui::TouchEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(beng): Set Focus.
  return ui::ER_UNHANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, aura::WindowObserver implementation:

void FocusController::OnWindowVisibilityChanged(aura::Window* window,
                                                bool visible) {
  // TODO(beng): determine if we need to use Contains() here.
  if (window == focused_window_) {
    // TODO(beng): Reset focused window.
  }
  if (window == active_window_) {
    // TODO(beng): Reset active window.
  }
}

void FocusController::OnWindowDestroyed(aura::Window* window) {
  if (window == focused_window_) {
    // TODO(beng): Reset focused window.
  }
  if (window == active_window_) {
    // TODO(beng): Reset active window.
  }
}

void FocusController::OnWillRemoveWindow(aura::Window* window) {
  if (window->Contains(focused_window_)) {
    // TODO(beng): Reset focused window.
  }
  if (window->Contains(active_window_)) {
    // TODO(beng): Reset active window.
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventDispatcher implementation:

bool FocusController::CanDispatchToTarget(ui::EventTarget* target) {
  // TODO(beng):
  return true;
}

}  // namespace corewm
}  // namespace views
