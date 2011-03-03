// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas_skia.h"
#include "views/focus/view_storage.h"
#include "views/layout/fill_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

#if defined(TOUCH_UI)
#include "views/touchui/gesture_manager.h"
#endif

namespace views {

// static
const char RootView::kViewClassName[] = "views/RootView";

////////////////////////////////////////////////////////////////////////////////
// RootView, public:

// Creation and lifetime -------------------------------------------------------

RootView::RootView(Widget* widget)
    : widget_(widget),
      mouse_pressed_handler_(NULL),
      mouse_move_handler_(NULL),
      last_click_handler_(NULL),
      explicit_mouse_handler_(false),
      last_mouse_event_flags_(0),
      last_mouse_event_x_(-1),
      last_mouse_event_y_(-1),
#if defined(TOUCH_UI)
      gesture_manager_(GestureManager::GetInstance()),
      touch_pressed_handler_(NULL),
#endif
      ALLOW_THIS_IN_INITIALIZER_LIST(focus_search_(this, false, false)),
      focus_traversable_parent_(NULL),
      focus_traversable_parent_view_(NULL) {
}

RootView::~RootView() {
  // If we have children remove them explicitly so to make sure a remove
  // notification is sent for each one of them.
  if (has_children())
    RemoveAllChildViews(true);
}

// Tree operations -------------------------------------------------------------

void RootView::SetContentsView(View* contents_view) {
  DCHECK(contents_view && GetWidget()->GetNativeView()) <<
      "Can't be called until after the native view is created!";
  // The ContentsView must be set up _after_ the window is created so that its
  // Widget pointer is valid.
  SetLayoutManager(new FillLayout);
  if (has_children())
    RemoveAllChildViews(true);
  AddChildView(contents_view);

  // Force a layout now, since the attached hierarchy won't be ready for the
  // containing window's bounds. Note that we call Layout directly rather than
  // calling the widget's size changed handler, since the RootView's bounds may
  // not have changed, which will cause the Layout not to be done otherwise.
  Layout();
}

void RootView::NotifyNativeViewHierarchyChanged(bool attached,
                                                gfx::NativeView native_view) {
  PropagateNativeViewHierarchyChanged(attached, native_view, this);
}

// Input -----------------------------------------------------------------------

void RootView::ProcessMouseDragCanceled() {
  if (mouse_pressed_handler_) {
    // Synthesize a release event.
    MouseEvent release_event(ui::ET_MOUSE_RELEASED, last_mouse_event_x_,
      last_mouse_event_y_, last_mouse_event_flags_);
    OnMouseReleased(release_event, true);
  }
}

void RootView::ProcessOnMouseExited() {
  if (mouse_move_handler_ != NULL) {
    MouseEvent exited_event(ui::ET_MOUSE_EXITED, 0, 0, 0);
    mouse_move_handler_->OnMouseExited(exited_event);
    mouse_move_handler_ = NULL;
  }
}

bool RootView::ProcessKeyEvent(const KeyEvent& event) {
  bool consumed = false;

  View* v = GetFocusManager()->GetFocusedView();
  // Special case to handle right-click context menus triggered by the
  // keyboard.
  if (v && v->IsEnabled() && ((event.key_code() == ui::VKEY_APPS) ||
     (event.key_code() == ui::VKEY_F10 && event.IsShiftDown()))) {
    v->ShowContextMenu(v->GetKeyboardContextMenuLocation(), false);
    return true;
  }
  for (; v && v != this && !consumed; v = v->parent()) {
    consumed = (event.type() == ui::ET_KEY_PRESSED) ?
        v->OnKeyPressed(event) : v->OnKeyReleased(event);
  }
  return consumed;
}

// Focus -----------------------------------------------------------------------

void RootView::SetFocusTraversableParent(FocusTraversable* focus_traversable) {
  DCHECK(focus_traversable != this);
  focus_traversable_parent_ = focus_traversable;
}

void RootView::SetFocusTraversableParentView(View* view) {
  focus_traversable_parent_view_ = view;
}

// System events ---------------------------------------------------------------

void RootView::ThemeChanged() {
  View::PropagateThemeChanged();
}

void RootView::LocaleChanged() {
  View::PropagateLocaleChanged();
}

////////////////////////////////////////////////////////////////////////////////
// RootView, FocusTraversable implementation:

FocusSearch* RootView::GetFocusSearch() {
  return &focus_search_;
}

FocusTraversable* RootView::GetFocusTraversableParent() {
  return focus_traversable_parent_;
}

View* RootView::GetFocusTraversableParentView() {
  return focus_traversable_parent_view_;
}

////////////////////////////////////////////////////////////////////////////////
// RootView, View overrides:

void RootView::SchedulePaintInRect(const gfx::Rect& rect) {
  gfx::Rect xrect = ConvertRectToParent(rect);
  gfx::Rect invalid_rect = GetLocalBounds().Intersect(xrect);
  if (!invalid_rect.IsEmpty())
    widget_->SchedulePaintInRect(invalid_rect);
}

const Widget* RootView::GetWidget() const {
  return widget_;
}

Widget* RootView::GetWidget() {
  return const_cast<Widget*>(const_cast<const RootView*>(this)->GetWidget());
}

bool RootView::OnMousePressed(const MouseEvent& event) {
  MouseEvent e(event, this);

  // This function does not normally handle non-client messages except for
  // non-client double-clicks. Actually, all double-clicks are special as the
  // are formed from a single-click followed by a double-click event. When the
  // double-click event lands on a different view than its single-click part,
  // we transform it into a single-click which prevents odd things.
  if ((e.flags() & ui::EF_IS_NON_CLIENT) &&
      !(e.flags() & ui::EF_IS_DOUBLE_CLICK)) {
    last_click_handler_ = NULL;
    return false;
  }

  UpdateCursor(e);
  SetMouseLocationAndFlags(e);

  // If mouse_pressed_handler_ is non null, we are currently processing
  // a pressed -> drag -> released session. In that case we send the
  // event to mouse_pressed_handler_
  if (mouse_pressed_handler_) {
    MouseEvent mouse_pressed_event(e, this, mouse_pressed_handler_);
    drag_info.Reset();
    mouse_pressed_handler_->ProcessMousePressed(mouse_pressed_event,
                                                &drag_info);
    return true;
  }
  DCHECK(!explicit_mouse_handler_);

  bool hit_disabled_view = false;
  // Walk up the tree until we find a view that wants the mouse event.
  for (mouse_pressed_handler_ = GetEventHandlerForPoint(e.location());
       mouse_pressed_handler_ && (mouse_pressed_handler_ != this);
       mouse_pressed_handler_ = mouse_pressed_handler_->parent()) {
    if (!mouse_pressed_handler_->IsEnabled()) {
      // Disabled views should eat events instead of propagating them upwards.
      hit_disabled_view = true;
      break;
    }

    // See if this view wants to handle the mouse press.
    MouseEvent mouse_pressed_event(e, this, mouse_pressed_handler_);

    // Remove the double-click flag if the handler is different than the
    // one which got the first click part of the double-click.
    if (mouse_pressed_handler_ != last_click_handler_)
      mouse_pressed_event.set_flags(e.flags() &
                                    ~ui::EF_IS_DOUBLE_CLICK);

    drag_info.Reset();
    bool handled = mouse_pressed_handler_->ProcessMousePressed(
        mouse_pressed_event, &drag_info);

    // The view could have removed itself from the tree when handling
    // OnMousePressed().  In this case, the removal notification will have
    // reset mouse_pressed_handler_ to NULL out from under us.  Detect this
    // case and stop.  (See comments in view.h.)
    //
    // NOTE: Don't return true here, because we don't want the frame to
    // forward future events to us when there's no handler.
    if (!mouse_pressed_handler_)
      break;

    // If the view handled the event, leave mouse_pressed_handler_ set and
    // return true, which will cause subsequent drag/release events to get
    // forwarded to that view.
    if (handled) {
      last_click_handler_ = mouse_pressed_handler_;
      return true;
    }
  }

  // Reset mouse_pressed_handler_ to indicate that no processing is occurring.
  mouse_pressed_handler_ = NULL;

  // In the event that a double-click is not handled after traversing the
  // entire hierarchy (even as a single-click when sent to a different view),
  // it must be marked as handled to avoid anything happening from default
  // processing if it the first click-part was handled by us.
  if (last_click_handler_ && e.flags() & ui::EF_IS_DOUBLE_CLICK)
    hit_disabled_view = true;

  last_click_handler_ = NULL;
  return hit_disabled_view;
}

bool RootView::OnMouseDragged(const MouseEvent& event) {
  MouseEvent e(event, this);
  UpdateCursor(e);

  if (mouse_pressed_handler_) {
    SetMouseLocationAndFlags(e);

    gfx::Point p;
    ConvertPointToMouseHandler(e.location(), &p);
    MouseEvent mouse_event(e.type(), p.x(), p.y(), e.flags());
    return mouse_pressed_handler_->ProcessMouseDragged(mouse_event, &drag_info);
  }
  return false;
}

void RootView::OnMouseReleased(const MouseEvent& event, bool canceled) {
  MouseEvent e(event, this);
  UpdateCursor(e);

  if (mouse_pressed_handler_) {
    gfx::Point p;
    ConvertPointToMouseHandler(e.location(), &p);
    MouseEvent mouse_released(e.type(), p.x(), p.y(), e.flags());
    // We allow the view to delete us from ProcessMouseReleased. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;
    mouse_pressed_handler_ = NULL;
    explicit_mouse_handler_ = false;
    mouse_pressed_handler->ProcessMouseReleased(mouse_released, canceled);
    // WARNING: we may have been deleted.
  }
}

void RootView::OnMouseMoved(const MouseEvent& event) {
  MouseEvent e(event, this);
  View* v = GetEventHandlerForPoint(e.location());
  // Find the first enabled view, or the existing move handler, whichever comes
  // first.  The check for the existing handler is because if a view becomes
  // disabled while handling moves, it's wrong to suddenly send ET_MOUSE_EXITED
  // and ET_MOUSE_ENTERED events, because the mouse hasn't actually exited yet.
  while (v && !v->IsEnabled() && (v != mouse_move_handler_))
    v = v->parent();
  if (v && v != this) {
    if (v != mouse_move_handler_) {
      if (mouse_move_handler_ != NULL) {
        MouseEvent exited_event(ui::ET_MOUSE_EXITED, 0, 0, 0);
        mouse_move_handler_->OnMouseExited(exited_event);
      }

      mouse_move_handler_ = v;

      MouseEvent entered_event(ui::ET_MOUSE_ENTERED,
                               this,
                               mouse_move_handler_,
                               e.location(),
                               0);
      mouse_move_handler_->OnMouseEntered(entered_event);
    }
    MouseEvent moved_event(ui::ET_MOUSE_MOVED,
                           this,
                           mouse_move_handler_,
                           e.location(),
                           0);
    mouse_move_handler_->OnMouseMoved(moved_event);

    gfx::NativeCursor cursor = mouse_move_handler_->GetCursorForPoint(
        moved_event.type(), moved_event.location());
    widget_->SetCursor(cursor);
  } else if (mouse_move_handler_ != NULL) {
    MouseEvent exited_event(ui::ET_MOUSE_EXITED, 0, 0, 0);
    mouse_move_handler_->OnMouseExited(exited_event);
    widget_->SetCursor(NULL);
  }
}

void RootView::SetMouseHandler(View *new_mh) {
  // If we're clearing the mouse handler, clear explicit_mouse_handler as well.
  explicit_mouse_handler_ = (new_mh != NULL);
  mouse_pressed_handler_ = new_mh;
}

bool RootView::OnMouseWheel(const MouseWheelEvent& event) {
  MouseWheelEvent e(event, this);
  bool consumed = false;
  View* v = GetFocusManager()->GetFocusedView();
  for (; v && v != this && !consumed; v = v->parent())
    consumed = v->OnMouseWheel(e);
  return consumed;
}

#if defined(TOUCH_UI)
View::TouchStatus RootView::OnTouchEvent(const TouchEvent& event) {
  TouchEvent e(event, this);

  // If touch_pressed_handler_ is non null, we are currently processing
  // a touch down on the screen situation. In that case we send the
  // event to touch_pressed_handler_
  View::TouchStatus status = TOUCH_STATUS_UNKNOWN;

  if (touch_pressed_handler_) {
    TouchEvent touch_event(e, this, touch_pressed_handler_);
    status = touch_pressed_handler_->ProcessTouchEvent(touch_event);
    gesture_manager_->ProcessTouchEventForGesture(e, this, status);
    if (status == TOUCH_STATUS_END)
      touch_pressed_handler_ = NULL;
    return status;
  }

  // Walk up the tree until we find a view that wants the touch event.
  for (touch_pressed_handler_ = GetEventHandlerForPoint(e.location());
       touch_pressed_handler_ && (touch_pressed_handler_ != this);
       touch_pressed_handler_ = touch_pressed_handler_->parent()) {
    if (!touch_pressed_handler_->IsEnabled()) {
      // Disabled views eat events but are treated as not handled by the
      // the GestureManager.
      status = TOUCH_STATUS_UNKNOWN;
      break;
    }

    // See if this view wants to handle the touch
    TouchEvent touch_event(e, this, touch_pressed_handler_);
    status = touch_pressed_handler_->ProcessTouchEvent(touch_event);

    // If the touch didn't initiate a touch-sequence, then reset the touch event
    // handler.
    if (status != TOUCH_STATUS_START)
      touch_pressed_handler_ = NULL;

    // The view could have removed itself from the tree when handling
    // OnTouchEvent(). So handle as per OnMousePressed. NB: we
    // assume that the RootView itself cannot be so removed.
    //
    // NOTE: Don't return true here, because we don't want the frame to
    // forward future events to us when there's no handler.
    if (!touch_pressed_handler_)
      break;

    // If the view handled the event, leave touch_pressed_handler_ set and
    // return true, which will cause subsequent drag/release events to get
    // forwarded to that view.
    if (status != TOUCH_STATUS_UNKNOWN) {
      gesture_manager_->ProcessTouchEventForGesture(e, this, status);
      return status;
    }
  }

  // Reset touch_pressed_handler_ to indicate that no processing is occurring.
  touch_pressed_handler_ = NULL;

  // Give the touch event to the gesture manager.
  gesture_manager_->ProcessTouchEventForGesture(e, this, status);
  return status;
}
#endif

bool RootView::IsVisibleInRootView() const {
  return IsVisible();
}

std::string RootView::GetClassName() const {
  return kViewClassName;
}

void RootView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_APPLICATION;
}

void RootView::OnPaint(gfx::Canvas* canvas) {
  canvas->AsCanvasSkia()->drawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);
}

void RootView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  widget_->ViewHierarchyChanged(is_add, parent, child);

  if (!is_add) {
    if (!explicit_mouse_handler_ && mouse_pressed_handler_ == child)
      mouse_pressed_handler_ = NULL;
    if (mouse_move_handler_ == child)
      mouse_move_handler_ = NULL;
#if defined(TOUCH_UI)
    if (touch_pressed_handler_)
      touch_pressed_handler_ = NULL;
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootView, protected:

// Coordinate conversion -------------------------------------------------------

bool RootView::ConvertPointToMouseHandler(const gfx::Point& l,
                                          gfx::Point* p) {
  //
  // If the mouse_handler was set explicitly, we need to keep
  // sending events even if it was re-parented in a different
  // window. (a non explicit mouse handler is automatically
  // cleared when the control is removed from the hierarchy)
  if (explicit_mouse_handler_) {
    if (mouse_pressed_handler_->GetWidget()) {
      *p = l;
      ConvertPointToScreen(this, p);
      ConvertPointToView(NULL, mouse_pressed_handler_, p);
    } else {
      // If the mouse_pressed_handler_ is not connected, we send the
      // event in screen coordinate system
      *p = l;
      ConvertPointToScreen(this, p);
      return true;
    }
  } else {
    *p = l;
    ConvertPointToView(this, mouse_pressed_handler_, p);
  }
  return true;
}

// Input -----------------------------------------------------------------------

void RootView::UpdateCursor(const MouseEvent& e) {
  gfx::NativeCursor cursor = NULL;
  View* v = GetEventHandlerForPoint(e.location());
  if (v && v != this) {
    gfx::Point l(e.location());
    View::ConvertPointToView(this, v, &l);
    cursor = v->GetCursorForPoint(e.type(), l);
  }
  widget_->SetCursor(cursor);
}

void RootView::SetMouseLocationAndFlags(const MouseEvent& e) {
  last_mouse_event_flags_ = e.flags();
  last_mouse_event_x_ = e.x();
  last_mouse_event_y_ = e.y();
}

}  // namespace views
