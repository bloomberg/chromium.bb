// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/custom_button.h"

#include "ui/base/animation/throb_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/screen.h"

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
    } else if (state_ == BS_HOT && state == BS_NORMAL) {
      // Button is returning to a normal state from hover, start hover
      // fade animation.
      hover_animation_->Hide();
    } else {
      hover_animation_->Stop();
    }
  }

  state_ = state;
  SchedulePaint();
}

void CustomButton::StartThrobbing(int cycles_til_stop) {
  is_throbbing_ = true;
  hover_animation_->StartThrobbing(cycles_til_stop);
}

void CustomButton::SetAnimationDuration(int duration) {
  hover_animation_->SetSlideDuration(duration);
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides:

AccessibilityTypes::State CustomButton::GetAccessibleState() {
  int state = 0;
  switch (state_) {
    case BS_HOT:
      state = AccessibilityTypes::STATE_HOTTRACKED;
      break;
    case BS_PUSHED:
      state = AccessibilityTypes::STATE_PRESSED;
      break;
    case BS_DISABLED:
      state = AccessibilityTypes::STATE_UNAVAILABLE;
      break;
    case BS_NORMAL:
    case BS_COUNT:
      // No additional accessibility state set for this button state.
      break;
  }
  return state;
}

void CustomButton::SetEnabled(bool enabled) {
  if (enabled ? (state_ != BS_DISABLED) : (state_ == BS_DISABLED))
    return;

  if (enabled)
    SetState(IsMouseHovered() ? BS_HOT : BS_NORMAL);
  else
    SetState(BS_DISABLED);
}

bool CustomButton::IsEnabled() const {
  return state_ != BS_DISABLED;
}

bool CustomButton::IsFocusable() const {
  return (state_ != BS_DISABLED) && View::IsFocusable();
}

bool CustomButton::IsMouseHovered() const {
  // If we haven't yet been placed in an onscreen view hierarchy, we can't be
  // hovered.
  if (!GetWidget())
    return false;

  gfx::Point cursor_pos(Screen::GetCursorScreenPoint());
  ConvertPointToView(NULL, this, &cursor_pos);
  return HitTest(cursor_pos);
}

std::string CustomButton::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, protected:

CustomButton::CustomButton(ButtonListener* listener)
    : Button(listener),
      state_(BS_NORMAL),
      animate_on_state_change_(true),
      is_throbbing_(false),
      triggerable_event_flags_(ui::EF_LEFT_BUTTON_DOWN),
      request_focus_on_press_(true) {
  hover_animation_.reset(new ui::ThrobAnimation(this));
  hover_animation_->SetSlideDuration(kHoverFadeDurationMs);
}

bool CustomButton::IsTriggerableEvent(const MouseEvent& e) {
  return (triggerable_event_flags_ & e.flags()) != 0;
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides (protected):

bool CustomButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (!enabled_)
    return false;

  SetState(BS_NORMAL);
  KeyEvent key_event(ui::ET_KEY_RELEASED, accelerator.GetKeyCode(),
                     accelerator.modifiers(), 0, 0);
  NotifyClick(key_event);
  return true;
}

bool CustomButton::OnMousePressed(const MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (ShouldEnterPushedState(e) && HitTest(e.location()))
      SetState(BS_PUSHED);
    if (request_focus_on_press_)
      RequestFocus();
  }
  return true;
}

bool CustomButton::OnMouseDragged(const MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (HitTest(e.location()))
      SetState(ShouldEnterPushedState(e) ? BS_PUSHED : BS_HOT);
    else
      SetState(BS_NORMAL);
  }
  return true;
}

void CustomButton::OnMouseReleased(const MouseEvent& e, bool canceled) {
  // Starting a drag results in a MouseReleased, we need to ignore it.
  if ((state_ == BS_DISABLED) || InDrag())
    return;

  if (!HitTest(e.location())) {
    SetState(BS_NORMAL);
    return;
  }

  SetState(BS_HOT);
  if (!canceled && IsTriggerableEvent(e)) {
    NotifyClick(e);
    // NOTE: We may be deleted at this point (by the listener's notification
    // handler).
  }
}

void CustomButton::OnMouseEntered(const MouseEvent& e) {
  if (state_ != BS_DISABLED)
    SetState(BS_HOT);
}

void CustomButton::OnMouseMoved(const MouseEvent& e) {
  if (state_ != BS_DISABLED)
    SetState(HitTest(e.location()) ? BS_HOT : BS_NORMAL);
}

void CustomButton::OnMouseExited(const MouseEvent& e) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (state_ != BS_DISABLED && !InDrag())
    SetState(BS_NORMAL);
}

bool CustomButton::OnKeyPressed(const KeyEvent& e) {
  if (state_ == BS_DISABLED)
    return false;

  // Space sets button state to pushed. Enter clicks the button. This matches
  // the Windows native behavior of buttons, where Space clicks the button on
  // KeyRelease and Enter clicks the button on KeyPressed.
  if (e.key_code() == ui::VKEY_SPACE) {
    SetState(BS_PUSHED);
  } else if (e.key_code() == ui::VKEY_RETURN) {
    SetState(BS_NORMAL);
    NotifyClick(e);
  } else {
    return false;
  }
  return true;
}

bool CustomButton::OnKeyReleased(const KeyEvent& e) {
  if ((state_ == BS_DISABLED) || (e.key_code() != ui::VKEY_SPACE))
    return false;

  SetState(BS_NORMAL);
  NotifyClick(e);
  return true;
}

void CustomButton::OnDragDone() {
  SetState(BS_NORMAL);
}

void CustomButton::ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture) {
  if (!GetContextMenuController())
    return;

  // We're about to show the context menu. Showing the context menu likely means
  // we won't get a mouse exited and reset state. Reset it now to be sure.
  if (state_ != BS_DISABLED)
    SetState(BS_NORMAL);
  View::ShowContextMenu(p, is_mouse_gesture);
}

void CustomButton::ViewHierarchyChanged(bool is_add, View *parent,
                                        View *child) {
  if (!is_add && state_ != BS_DISABLED)
    SetState(BS_NORMAL);
}

void CustomButton::SetHotTracked(bool flag) {
  if (state_ != BS_DISABLED)
    SetState(flag ? BS_HOT : BS_NORMAL);

  if (flag)
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_FOCUS);
}

bool CustomButton::IsHotTracked() const {
  return state_ == BS_HOT;
}

void CustomButton::WillLoseFocus() {
  if (IsHotTracked())
    SetState(BS_NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, ui::AnimationDelegate implementation:

void CustomButton::AnimationProgressed(const ui::Animation* animation) {
  SchedulePaint();
}

bool CustomButton::ShouldEnterPushedState(const MouseEvent& e) {
  return IsTriggerableEvent(e);
}

}  // namespace views
