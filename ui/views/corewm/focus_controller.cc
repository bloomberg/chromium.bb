// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_controller.h"

#include "base/auto_reset.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/views/corewm/focus_change_event.h"
#include "ui/views/corewm/focus_rules.h"

namespace views {
namespace corewm {
namespace {

// When a modal window is activated, we bring its entire transient parent chain
// to the front. This function must be called before the modal transient is
// stacked at the top to ensure correct stacking order.
void StackTransientParentsBelowModalWindow(aura::Window* window) {
  if (window->GetProperty(aura::client::kModalKey) != ui::MODAL_TYPE_WINDOW)
    return;

  aura::Window* transient_parent = window->transient_parent();
  while (transient_parent) {
    transient_parent->parent()->StackChildAtTop(transient_parent);
    transient_parent = transient_parent->transient_parent();
  }
}

// Updates focused window state and dispatches changing/changed events.
void DispatchEventsAndUpdateState(ui::EventDispatcher* dispatcher,
                                  int changing_event_type,
                                  int changed_event_type,
                                  aura::Window** state,
                                  aura::Window* new_state,
                                  bool restack,
                                  ui::EventTarget** event_dispatch_target) {
  int result = ui::ER_UNHANDLED;
  {
    base::AutoReset<ui::EventTarget*> reset(event_dispatch_target, *state);
    FocusChangeEvent changing_event(changing_event_type);
    result = dispatcher->ProcessEvent(*state, &changing_event);
  }
  DCHECK(!(result & ui::ER_CONSUMED))
      << "Focus and Activation events cannot be consumed";

  *state = new_state;

  if (restack && new_state) {
    StackTransientParentsBelowModalWindow(new_state);
    new_state->parent()->StackChildAtTop(new_state);
  }

  {
    base::AutoReset<ui::EventTarget*> reset(event_dispatch_target, *state);
    FocusChangeEvent changed_event(changed_event_type);
    dispatcher->ProcessEvent(*state, &changed_event);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FocusController, public:

FocusController::FocusController(FocusRules* rules)
    : active_window_(NULL),
      focused_window_(NULL),
      event_dispatch_target_(NULL),
      rules_(rules) {
  DCHECK(rules);
  FocusChangeEvent::RegisterEventTypes();
  aura::Env::GetInstance()->AddObserver(this);
}

FocusController::~FocusController() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void FocusController::FocusWindow(aura::Window* window) {
  // Focusing a window also activates its containing activatable window. Note
  // that the rules could redirect activation activation and/or focus.
  aura::Window* focusable = rules_->GetFocusableWindow(window);
  SetActiveWindow(rules_->GetActivatableWindow(focusable));
  DCHECK(GetActiveWindow()->Contains(focusable));
  SetFocusedWindow(focusable);
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
  FocusWindow(window);
}

void FocusController::DeactivateWindow(aura::Window* window) {
  FocusWindow(rules_->GetNextActivatableWindow(window));
}

aura::Window* FocusController::GetActiveWindow() {
  return active_window_;
}

aura::Window* FocusController::GetActivatableWindow(aura::Window* window) {
  return rules_->GetActivatableWindow(window);
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
  if (event->type() == ui::ET_MOUSE_PRESSED)
    WindowFocusedFromInputEvent(static_cast<aura::Window*>(event->target()));
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnTouchEvent(ui::TouchEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult FocusController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    WindowFocusedFromInputEvent(static_cast<aura::Window*>(event->target()));
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
  if (!visible)
    WindowLostFocusFromDispositionChange(window);
}

void FocusController::OnWindowDestroying(aura::Window* window) {
  WindowLostFocusFromDispositionChange(window);
}

void FocusController::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

void FocusController::OnWillRemoveWindow(aura::Window* window) {
  WindowLostFocusFromDispositionChange(window);
}

void FocusController::OnWindowInitialized(aura::Window* window) {
  window->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventDispatcher implementation:

bool FocusController::CanDispatchToTarget(ui::EventTarget* target) {
  return target == event_dispatch_target_;
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, private:

void FocusController::SetFocusedWindow(aura::Window* window) {
  if (window == focused_window_)
    return;
  DCHECK(rules_->CanFocusWindow(window));
  if (window)
    DCHECK_EQ(window, rules_->GetFocusableWindow(window));
  DispatchEventsAndUpdateState(
      this,
      FocusChangeEvent::focus_changing_event_type(),
      FocusChangeEvent::focus_changed_event_type(),
      &focused_window_,
      window,
      /* restack */ false,
      &event_dispatch_target_);
}

void FocusController::SetActiveWindow(aura::Window* window) {
  if (window == active_window_)
    return;
  DCHECK(rules_->CanActivateWindow(window));
  if (window)
    DCHECK_EQ(window, rules_->GetActivatableWindow(window));
  DispatchEventsAndUpdateState(
      this,
      FocusChangeEvent::activation_changing_event_type(),
      FocusChangeEvent::activation_changed_event_type(),
      &active_window_,
      window,
      /* restack */ true,
      &event_dispatch_target_);
}

void FocusController::WindowLostFocusFromDispositionChange(
    aura::Window* window) {
  // TODO(beng): See if this function can be replaced by a call to
  //             FocusWindow().
  // Activation adjustments are handled first in the event of a disposition
  // changed. If an activation change is necessary, focus is reset as part of
  // that process so there's no point in updating focus independently.
  if (window->Contains(active_window_)) {
    aura::Window* next_activatable = rules_->GetNextActivatableWindow(window);
    SetActiveWindow(next_activatable);
    SetFocusedWindow(next_activatable);
  } else if (window->Contains(focused_window_)) {
    // Active window isn't changing, but focused window might be.
    SetFocusedWindow(rules_->GetNextFocusableWindow(window));
  }
}

void FocusController::WindowFocusedFromInputEvent(aura::Window* window) {
  FocusWindow(window);
}

}  // namespace corewm
}  // namespace views
