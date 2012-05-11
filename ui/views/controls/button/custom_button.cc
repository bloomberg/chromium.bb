// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/custom_button.h"

#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace views {

// How long the hover animation takes if uninterrupted.
static const int kHoverFadeDurationMs = 150;

// static
const char CustomButton::kViewClassName[] = "views/CustomButton";

////////////////////////////////////////////////////////////////////////////////
// CustomButton, public:

CustomButton::~CustomButton() {
}

void CustomButton::SetState(ButtonState state) {
  if (state == state_)
    return;

  if (animate_on_state_change_ &&
      (!is_throbbing_ || !hover_animation_->is_animating())) {
    is_throbbing_ = false;
    if (state_ == BS_NORMAL && state == BS_HOT) {
      // Button is hovered from a normal state, start hover animation.
      hover_animation_->Show();
    } else if ((state_ == BS_HOT || state_ == BS_PUSHED)
          && state == BS_NORMAL) {
      // Button is returning to a normal state from hover, start hover
      // fade animation.
      hover_animation_->Hide();
    } else {
      hover_animation_->Stop();
    }
  }

  state_ = state;
  StateChanged();
  SchedulePaint();
}

void CustomButton::StartThrobbing(int cycles_til_stop) {
  is_throbbing_ = true;
  hover_animation_->StartThrobbing(cycles_til_stop);
}

void CustomButton::StopThrobbing() {
  if (hover_animation_->is_animating()) {
    hover_animation_->Stop();
    SchedulePaint();
  }
}

void CustomButton::SetAnimationDuration(int duration) {
  hover_animation_->SetSlideDuration(duration);
}

bool CustomButton::IsMouseHovered() const {
  // If we haven't yet been placed in an onscreen view hierarchy, we can't be
  // hovered.
  if (!GetWidget())
    return false;

  gfx::Point cursor_pos(gfx::Screen::GetCursorScreenPoint());
  ConvertPointToView(NULL, this, &cursor_pos);
  return HitTest(cursor_pos);
}

void CustomButton::SetHotTracked(bool is_hot_tracked) {
  if (state_ != BS_DISABLED)
    SetState(is_hot_tracked ? BS_HOT : BS_NORMAL);

  if (is_hot_tracked && GetWidget()) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_FOCUS, true);
  }
}

bool CustomButton::IsHotTracked() const {
  return state_ == BS_HOT;
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides:

void CustomButton::OnEnabledChanged() {
  if (enabled() ? (state_ != BS_DISABLED) : (state_ == BS_DISABLED))
    return;

  if (enabled())
    SetState(IsMouseHovered() ? BS_HOT : BS_NORMAL);
  else
    SetState(BS_DISABLED);
}

std::string CustomButton::GetClassName() const {
  return kViewClassName;
}

bool CustomButton::OnMousePressed(const MouseEvent& event) {
  if (state_ != BS_DISABLED) {
    if (ShouldEnterPushedState(event) && HitTest(event.location()))
      SetState(BS_PUSHED);
    if (request_focus_on_press_)
      RequestFocus();
  }
  return true;
}

bool CustomButton::OnMouseDragged(const MouseEvent& event) {
  if (state_ != BS_DISABLED) {
    if (HitTest(event.location()))
      SetState(ShouldEnterPushedState(event) ? BS_PUSHED : BS_HOT);
    else
      SetState(BS_NORMAL);
  }
  return true;
}

void CustomButton::OnMouseReleased(const MouseEvent& event) {
  if (state_ == BS_DISABLED)
    return;

  if (!HitTest(event.location())) {
    SetState(BS_NORMAL);
    return;
  }

  SetState(BS_HOT);
  if (IsTriggerableEvent(event)) {
    NotifyClick(event);
    // NOTE: We may be deleted at this point (by the listener's notification
    // handler).
  }
}

void CustomButton::OnMouseCaptureLost() {
  // Starting a drag results in a MouseCaptureLost, we need to ignore it.
  if (state_ != BS_DISABLED && !InDrag())
    SetState(BS_NORMAL);
}

void CustomButton::OnMouseEntered(const MouseEvent& event) {
  if (state_ != BS_DISABLED)
    SetState(BS_HOT);
}

void CustomButton::OnMouseExited(const MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (state_ != BS_DISABLED && !InDrag())
    SetState(BS_NORMAL);
}

void CustomButton::OnMouseMoved(const MouseEvent& event) {
  if (state_ != BS_DISABLED)
    SetState(HitTest(event.location()) ? BS_HOT : BS_NORMAL);
}

bool CustomButton::OnKeyPressed(const KeyEvent& event) {
  if (state_ == BS_DISABLED)
    return false;

  // Space sets button state to pushed. Enter clicks the button. This matches
  // the Windows native behavior of buttons, where Space clicks the button on
  // KeyRelease and Enter clicks the button on KeyPressed.
  if (event.key_code() == ui::VKEY_SPACE) {
    SetState(BS_PUSHED);
  } else if (event.key_code() == ui::VKEY_RETURN) {
    SetState(BS_NORMAL);
    NotifyClick(event);
  } else {
    return false;
  }
  return true;
}

bool CustomButton::OnKeyReleased(const KeyEvent& event) {
  if ((state_ == BS_DISABLED) || (event.key_code() != ui::VKEY_SPACE))
    return false;

  SetState(BS_NORMAL);
  NotifyClick(event);
  return true;
}

bool CustomButton::AcceleratorPressed(const ui::Accelerator& accelerator) {
  SetState(BS_NORMAL);
  KeyEvent key_event(ui::ET_KEY_RELEASED, accelerator.key_code(),
                     accelerator.modifiers());
  NotifyClick(key_event);
  return true;
}

void CustomButton::ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture) {
  if (!context_menu_controller())
    return;

  // We're about to show the context menu. Showing the context menu likely means
  // we won't get a mouse exited and reset state. Reset it now to be sure.
  if (state_ != BS_DISABLED)
    SetState(BS_NORMAL);
  View::ShowContextMenu(p, is_mouse_gesture);
}

void CustomButton::OnDragDone() {
  SetState(BS_NORMAL);
}

void CustomButton::GetAccessibleState(ui::AccessibleViewState* state) {
  Button::GetAccessibleState(state);
  switch (state_) {
    case BS_HOT:
      state->state = ui::AccessibilityTypes::STATE_HOTTRACKED;
      break;
    case BS_PUSHED:
      state->state = ui::AccessibilityTypes::STATE_PRESSED;
      break;
    case BS_DISABLED:
      state->state = ui::AccessibilityTypes::STATE_UNAVAILABLE;
      break;
    case BS_NORMAL:
    case BS_COUNT:
      // No additional accessibility state set for this button state.
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, ui::AnimationDelegate implementation:

void CustomButton::AnimationProgressed(const ui::Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, protected:

CustomButton::CustomButton(ButtonListener* listener)
    : Button(listener),
      state_(BS_NORMAL),
      animate_on_state_change_(true),
      is_throbbing_(false),
      triggerable_event_flags_(ui::EF_LEFT_MOUSE_BUTTON),
      request_focus_on_press_(true) {
  hover_animation_.reset(new ui::ThrobAnimation(this));
  hover_animation_->SetSlideDuration(kHoverFadeDurationMs);
}

void CustomButton::StateChanged() {
}

bool CustomButton::IsTriggerableEvent(const MouseEvent& event) {
  return (triggerable_event_flags_ & event.flags()) != 0;
}

bool CustomButton::ShouldEnterPushedState(const MouseEvent& event) {
  return IsTriggerableEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides (protected):

void CustomButton::ViewHierarchyChanged(bool is_add, View *parent,
                                        View *child) {
  if (!is_add && state_ != BS_DISABLED)
    SetState(BS_NORMAL);
}

void CustomButton::OnBlur() {
  if (IsHotTracked())
    SetState(BS_NORMAL);
}

}  // namespace views
