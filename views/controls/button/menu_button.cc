// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/menu_button.h"

#include "base/utf_string_conversions.h"
#include "grit/app_strings.h"
#include "grit/app_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/events/event.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using base::Time;
using base::TimeDelta;

namespace views {

// The amount of time, in milliseconds, we wait before allowing another mouse
// pressed event to show the menu.
static const int64 kMinimumTimeBetweenButtonClicks = 100;

// How much padding to put on the left and right of the menu marker.
static const int kMenuMarkerPaddingLeft = 3;
static const int kMenuMarkerPaddingRight = -1;

// Default menu offset.
static const int kDefaultMenuOffsetX = -2;
static const int kDefaultMenuOffsetY = -4;

// static
const char MenuButton::kViewClassName[] = "views/MenuButton";

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(ButtonListener* listener,
                       const std::wstring& text,
                       ViewMenuDelegate* menu_delegate,
                       bool show_menu_marker)
    : TextButton(listener, text),
      menu_visible_(false),
      menu_offset_(kDefaultMenuOffsetX, kDefaultMenuOffsetY),
      menu_delegate_(menu_delegate),
      show_menu_marker_(show_menu_marker),
      menu_marker_(ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_MENU_DROPARROW)),
      destroyed_flag_(NULL) {
  set_alignment(TextButton::ALIGN_LEFT);
}

MenuButton::~MenuButton() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

gfx::Size MenuButton::GetPreferredSize() {
  gfx::Size prefsize = TextButton::GetPreferredSize();
  if (show_menu_marker_) {
    prefsize.Enlarge(menu_marker_->width() + kMenuMarkerPaddingLeft +
                         kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

void MenuButton::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  TextButton::PaintButton(canvas, mode);

  if (show_menu_marker_) {
    gfx::Insets insets = GetInsets();

    // We can not use the views' mirroring infrastructure for mirroring a
    // MenuButton control (see TextButton::OnPaint() for a detailed explanation
    // regarding why we can not flip the canvas). Therefore, we need to
    // manually mirror the position of the down arrow.
    gfx::Rect arrow_bounds(width() - insets.right() -
                           menu_marker_->width() - kMenuMarkerPaddingRight,
                           height() / 2 - menu_marker_->height() / 2,
                           menu_marker_->width(),
                           menu_marker_->height());
    arrow_bounds.set_x(GetMirroredXForRect(arrow_bounds));
    canvas->DrawBitmapInt(*menu_marker_, arrow_bounds.x(), arrow_bounds.y());
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

int MenuButton::GetMaximumScreenXCoordinate() {
  if (!GetWidget()) {
    NOTREACHED();
    return 0;
  }

  gfx::Rect monitor_bounds =
      Screen::GetMonitorWorkAreaNearestWindow(GetWidget()->GetNativeView());
  return monitor_bounds.right() - 1;
}

bool MenuButton::Activate() {
  SetState(BS_PUSHED);
  // We need to synchronously paint here because subsequently we enter a
  // menu modal loop which will stop this window from updating and
  // receiving the paint message that should be spawned by SetState until
  // after the menu closes.
  PaintNow();
  if (menu_delegate_) {
    gfx::Rect lb = GetLocalBounds();

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (base::i18n::IsRTL())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(this, &menu_position);
    if (base::i18n::IsRTL())
      menu_position.Offset(-menu_offset_.x(), menu_offset_.y());
    else
      menu_position.Offset(menu_offset_.x(), menu_offset_.y());

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    GetRootView()->SetMouseHandler(NULL);

    menu_visible_ = true;

    bool destroyed = false;
    destroyed_flag_ = &destroyed;

    menu_delegate_->RunMenu(this, menu_position);

    if (destroyed) {
      // The menu was deleted while showing. Don't attempt any processing.
      return false;
    }

    destroyed_flag_ = NULL;

    menu_visible_ = false;
    menu_closed_time_ = Time::Now();

    // Now that the menu has closed, we need to manually reset state to
    // "normal" since the menu modal loop will have prevented normal
    // mouse move messages from getting to this View. We set "normal"
    // and not "hot" because the likelihood is that the mouse is now
    // somewhere else (user clicked elsewhere on screen to close the menu
    // or selected an item) and we will inevitably refresh the hot state
    // in the event the mouse _is_ over the view.
    SetState(BS_NORMAL);

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }
  return true;
}

bool MenuButton::OnMousePressed(const MouseEvent& e) {
  RequestFocus();
  if (state() != BS_DISABLED) {
    // If we're draggable (GetDragOperations returns a non-zero value), then
    // don't pop on press, instead wait for release.
    if (e.IsOnlyLeftMouseButton() && HitTest(e.location()) &&
        GetDragOperations(e.location()) == ui::DragDropTypes::DRAG_NONE) {
      TimeDelta delta = Time::Now() - menu_closed_time_;
      int64 delta_in_milliseconds = delta.InMilliseconds();
      if (delta_in_milliseconds > kMinimumTimeBetweenButtonClicks) {
        return Activate();
      }
    }
  }
  return true;
}

void MenuButton::OnMouseReleased(const MouseEvent& e,
                                 bool canceled) {
  // Explicitly test for left mouse button to show the menu. If we tested for
  // !IsTriggerableEvent it could lead to a situation where we end up showing
  // the menu and context menu (this would happen if the right button is not
  // triggerable and there's a context menu).
  if (GetDragOperations(e.location()) != ui::DragDropTypes::DRAG_NONE &&
      state() != BS_DISABLED && !canceled && !InDrag() &&
      e.IsOnlyLeftMouseButton() && HitTest(e.location())) {
    Activate();
  } else {
    TextButton::OnMouseReleased(e, canceled);
  }
}

bool MenuButton::OnKeyPressed(const KeyEvent& e) {
  switch (e.key_code()) {
    case ui::VKEY_SPACE:
    case ui::VKEY_RETURN:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      bool result = Activate();
      if (GetFocusManager()->GetFocusedView() == NULL)
        RequestFocus();
      return result;
    }
    default:
      break;
  }
  return false;
}

bool MenuButton::OnKeyReleased(const KeyEvent& e) {
  // Override CustomButton's implementation, which presses the button when
  // you press space and clicks it when you release space.  For a MenuButton
  // we always activate the menu on key press.
  return false;
}

// The reason we override View::OnMouseExited is because we get this event when
// we display the menu. If we don't override this method then
// BaseButton::OnMouseExited will get the event and will set the button's state
// to BS_NORMAL instead of keeping the state BM_PUSHED. This, in turn, will
// cause the button to appear depressed while the menu is displayed.
void MenuButton::OnMouseExited(const MouseEvent& event) {
  if ((state_ != BS_DISABLED) && (!menu_visible_) && (!InDrag())) {
    SetState(BS_NORMAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - accessibility
//
////////////////////////////////////////////////////////////////////////////////

void MenuButton::GetAccessibleState(ui::AccessibleViewState* state) {
  CustomButton::GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_BUTTONMENU;
  state->default_action = l10n_util::GetStringUTF16(IDS_APP_ACCACTION_PRESS);
  state->state = ui::AccessibilityTypes::STATE_HASPOPUP;
}

std::string MenuButton::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
