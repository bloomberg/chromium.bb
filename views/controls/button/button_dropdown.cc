// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/button_dropdown.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "grit/app_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/widget/widget.h"

namespace views {

// How long to wait before showing the menu
static const int kMenuTimerDelay = 500;

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - constructors, destructors, initialization, cleanup
//
////////////////////////////////////////////////////////////////////////////////

ButtonDropDown::ButtonDropDown(ButtonListener* listener,
                               ui::MenuModel* model)
    : ImageButton(listener),
      model_(model),
      y_position_on_lbuttondown_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_menu_factory_(this)) {
}

ButtonDropDown::~ButtonDropDown() {
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Events
//
////////////////////////////////////////////////////////////////////////////////

bool ButtonDropDown::OnMousePressed(const MouseEvent& e) {
  if (IsEnabled() && IsTriggerableEvent(e) && HitTest(e.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = e.y();

    // Schedule a task that will show the menu.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        show_menu_factory_.NewRunnableMethod(&ButtonDropDown::ShowDropDownMenu,
                                             GetWidget()->GetNativeView()),
        kMenuTimerDelay);
  }
  return ImageButton::OnMousePressed(e);
}

void ButtonDropDown::OnMouseReleased(const MouseEvent& e, bool canceled) {
  if (IsTriggerableEvent(e) ||
      (e.IsRightMouseButton() && !HitTest(e.location()))) {
    ImageButton::OnMouseReleased(e, canceled);
  }

  if (canceled)
    return;

  if (IsTriggerableEvent(e))
    show_menu_factory_.RevokeAll();

  if (IsEnabled() && e.IsRightMouseButton() && HitTest(e.location())) {
    show_menu_factory_.RevokeAll();
    ShowDropDownMenu(GetWidget()->GetNativeView());
    // Set the state back to normal after the drop down menu is closed.
    if (state_ != BS_DISABLED)
      SetState(BS_NORMAL);
  }
}

bool ButtonDropDown::OnMouseDragged(const MouseEvent& e) {
  bool result = ImageButton::OnMouseDragged(e);

  if (!show_menu_factory_.empty()) {
    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (e.y() > y_position_on_lbuttondown_ + GetHorizontalDragThreshold()) {
      show_menu_factory_.RevokeAll();
      ShowDropDownMenu(GetWidget()->GetNativeView());
    }
  }

  return result;
}

void ButtonDropDown::OnMouseExited(const MouseEvent& e) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  // A right click release triggers an exit event. We want to
  // remain in a PUSHED state until the drop down menu closes.
  if (state_ != BS_DISABLED && !InDrag() && state_ != BS_PUSHED)
    SetState(BS_NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Menu functions
//
////////////////////////////////////////////////////////////////////////////////

void ButtonDropDown::ShowContextMenu(const gfx::Point& p,
                                     bool is_mouse_gesture) {
  show_menu_factory_.RevokeAll();
  // Make the button look depressed while the menu is open.
  // NOTE: SetState() schedules a paint, but it won't occur until after the
  //       context menu message loop has terminated, so we PaintNow() to
  //       update the appearance synchronously.
  SetState(BS_PUSHED);
  PaintNow();
  ShowDropDownMenu(GetWidget()->GetNativeView());
  SetState(BS_HOT);
}

bool ButtonDropDown::ShouldEnterPushedState(const MouseEvent& e) {
  // Enter PUSHED state on press with Left or Right mouse button. Remain
  // in this state while the context menu is open.
  return ((MouseEvent::EF_LEFT_BUTTON_DOWN |
    MouseEvent::EF_RIGHT_BUTTON_DOWN) & e.GetFlags()) != 0;
}

void ButtonDropDown::ShowDropDownMenu(gfx::NativeView window) {
  if (model_) {
    gfx::Rect lb = GetLocalBounds(true);

    // Both the menu position and the menu anchor type change if the UI layout
    // is right-to-left.
    gfx::Point menu_position(lb.origin());
    menu_position.Offset(0, lb.height() - 1);
    if (base::i18n::IsRTL())
      menu_position.Offset(lb.width() - 1, 0);

    View::ConvertPointToScreen(this, &menu_position);

#if defined(OS_WIN)
    int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
#else
    int left_bound = 0;
    NOTIMPLEMENTED();
#endif
    if (menu_position.x() < left_bound)
      menu_position.set_x(left_bound);

    menu_.reset(new Menu2(model_));
    menu_->RunMenuAt(menu_position, Menu2::ALIGN_TOPLEFT);

    // Need to explicitly clear mouse handler so that events get sent
    // properly after the menu finishes running. If we don't do this, then
    // the first click to other parts of the UI is eaten.
    SetMouseHandler(NULL);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Accessibility
//
////////////////////////////////////////////////////////////////////////////////

string16 ButtonDropDown::GetAccessibleDefaultAction() {
  return l10n_util::GetStringUTF16(IDS_APP_ACCACTION_PRESS);
}

AccessibilityTypes::Role ButtonDropDown::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_BUTTONDROPDOWN;
}

AccessibilityTypes::State ButtonDropDown::GetAccessibleState() {
  return AccessibilityTypes::STATE_HASPOPUP;
}

}  // namespace views
