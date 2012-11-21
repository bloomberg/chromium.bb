// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_controller.h"

#include "ui/aura/env.h"
#include "ui/views/corewm/focus_change_event.h"
#include "ui/views/corewm/focus_rules.h"

namespace views {
namespace corewm {

////////////////////////////////////////////////////////////////////////////////
// FocusController, public:

FocusController::FocusController(FocusRules* rules)
    : active_window_(NULL),
      focused_window_(NULL),
      rules_(rules) {
  DCHECK(rules);
  FocusChangeEvent::RegisterEventTypes();
  aura::Env::GetInstance()->AddObserver(this);
}

FocusController::~FocusController() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void FocusController::SetFocusedWindow(aura::Window* window) {
  DCHECK(rules_->CanFocusWindow(window));
  // TODO(beng): dispatch changing events.
  FocusChangeEvent changing_event(
      FocusChangeEvent::focus_changing_event_type());
  int result = ProcessEvent(focused_window_, &changing_event);
  if (result & ui::ER_CONSUMED)
    return;

  focused_window_ = window;

  FocusChangeEvent changed_event(FocusChangeEvent::focus_changed_event_type());
  ProcessEvent(focused_window_, &changed_event);
}

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
  return rules_->CanActivateWindow(window);
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventHandler implementation:
ui::EventResult FocusController::OnKeyEvent(ui::KeyEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(beng): GetFocusableWindow().
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    SetFocusedWindow(rules_->GetFocusableWindow(
        static_cast<aura::Window*>(event->target())));
  }
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnTouchEvent(ui::TouchEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(beng): GetFocusableWindow().
  if (event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    SetFocusedWindow(rules_->GetFocusableWindow(
        static_cast<aura::Window*>(event->target())));
  }
  return ui::ER_UNHANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, aura::WindowObserver implementation:

void FocusController::OnWindowVisibilityChanging(aura::Window* window,
                                                 bool visible) {
  // We need to process this change in VisibilityChanging while the window is
  // still visible, since focus events cannot be dispatched to invisible
  // windows.
  // TODO(beng): evaluate whether or not the visibility restriction is worth
  //             enforcing for events that aren't user-input.
  if (window->Contains(focused_window_))
    SetFocusedWindow(rules_->GetNextFocusableWindow(window));

  if (window->Contains(active_window_)) {
    // TODO(beng): Reset active window.
  }
}

void FocusController::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);

  if (window->Contains(focused_window_))
    SetFocusedWindow(rules_->GetNextFocusableWindow(window));

  if (window->Contains(active_window_)) {
    // TODO(beng): Reset active window.
  }
}

void FocusController::OnWillRemoveWindow(aura::Window* window) {
  if (window->Contains(focused_window_))
    SetFocusedWindow(rules_->GetNextFocusableWindow(window));

  if (window->Contains(active_window_)) {
    // TODO(beng): Reset active window.
  }
}

void FocusController::OnWindowInitialized(aura::Window* window) {
  window->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventDispatcher implementation:

bool FocusController::CanDispatchToTarget(ui::EventTarget* target) {
  // TODO(beng):
  return true;
}

}  // namespace corewm
}  // namespace views
