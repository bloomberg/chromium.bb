// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace internal {

namespace {

enum EventType {
  EVENT_ENTER,
  EVENT_EXIT
};

// |view| is the view receiving |event|. This function sends the event to all
// the Views up the hierarchy that has |notify_enter_exit_on_child_| flag turned
// on, but does not contain |sibling|.
void NotifyEnterExitOfDescendant(const ui::MouseEvent& event,
                                 EventType type,
                                 View* view,
                                 View* sibling) {
  for (View* p = view->parent(); p; p = p->parent()) {
    if (!p->notify_enter_exit_on_child())
      continue;
    if (sibling && p->Contains(sibling))
        break;
    if (type == EVENT_ENTER)
      p->OnMouseEntered(event);
    else
      p->OnMouseExited(event);
  }
}

}  // namespace

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
      touch_pressed_handler_(NULL),
      gesture_handler_(NULL),
      scroll_gesture_handler_(NULL),
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
  DCHECK(contents_view && GetWidget()->native_widget()) <<
      "Can't be called until after the native widget is created!";
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

View* RootView::GetContentsView() {
  return child_count() > 0 ? child_at(0) : NULL;
}

void RootView::NotifyNativeViewHierarchyChanged(bool attached,
                                                gfx::NativeView native_view) {
  PropagateNativeViewHierarchyChanged(attached, native_view, this);
}

// Input -----------------------------------------------------------------------

bool RootView::OnKeyEvent(const ui::KeyEvent& event) {
  bool consumed = false;

  View* v = NULL;
  if (GetFocusManager())  // NULL in unittests.
    v = GetFocusManager()->GetFocusedView();
  // Special case to handle right-click context menus triggered by the
  // keyboard.
  if (v && v->enabled() && ((event.key_code() == ui::VKEY_APPS) ||
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

const Widget* RootView::GetWidget() const {
  return widget_;
}

Widget* RootView::GetWidget() {
  return const_cast<Widget*>(const_cast<const RootView*>(this)->GetWidget());
}

bool RootView::IsDrawn() const {
  return visible();
}

std::string RootView::GetClassName() const {
  return kViewClassName;
}

void RootView::SchedulePaintInRect(const gfx::Rect& rect) {
  if (layer()) {
    layer()->SchedulePaint(rect);
  } else {
    gfx::Rect xrect = ConvertRectToParent(rect);
    gfx::Rect invalid_rect = GetLocalBounds().Intersect(xrect);
    if (!invalid_rect.IsEmpty())
      widget_->SchedulePaintInRect(invalid_rect);
  }
}

bool RootView::OnMousePressed(const ui::MouseEvent& event) {
  UpdateCursor(event);
  SetMouseLocationAndFlags(event);

  // If mouse_pressed_handler_ is non null, we are currently processing
  // a pressed -> drag -> released session. In that case we send the
  // event to mouse_pressed_handler_
  if (mouse_pressed_handler_) {
    ui::MouseEvent mouse_pressed_event(event, static_cast<View*>(this),
                                       mouse_pressed_handler_);
    drag_info_.Reset();
    mouse_pressed_handler_->ProcessMousePressed(mouse_pressed_event,
                                                &drag_info_);
    return true;
  }
  DCHECK(!explicit_mouse_handler_);

  bool hit_disabled_view = false;
  // Walk up the tree until we find a view that wants the mouse event.
  for (mouse_pressed_handler_ = GetEventHandlerForPoint(event.location());
       mouse_pressed_handler_ && (mouse_pressed_handler_ != this);
       mouse_pressed_handler_ = mouse_pressed_handler_->parent()) {
    DVLOG(1) << "OnMousePressed testing "
        << mouse_pressed_handler_->GetClassName();
    if (!mouse_pressed_handler_->enabled()) {
      // Disabled views should eat events instead of propagating them upwards.
      hit_disabled_view = true;
      break;
    }

    // See if this view wants to handle the mouse press.
    ui::MouseEvent mouse_pressed_event(event, static_cast<View*>(this),
                                       mouse_pressed_handler_);

    // Remove the double-click flag if the handler is different than the
    // one which got the first click part of the double-click.
    if (mouse_pressed_handler_ != last_click_handler_)
      mouse_pressed_event.set_flags(event.flags() & ~ui::EF_IS_DOUBLE_CLICK);

    drag_info_.Reset();
    bool handled = mouse_pressed_handler_->ProcessMousePressed(
        mouse_pressed_event, &drag_info_);

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
      DVLOG(1) << "OnMousePressed handled by "
          << mouse_pressed_handler_->GetClassName();
      return true;
    }
  }

  // Reset mouse_pressed_handler_ to indicate that no processing is occurring.
  mouse_pressed_handler_ = NULL;

  // In the event that a double-click is not handled after traversing the
  // entire hierarchy (even as a single-click when sent to a different view),
  // it must be marked as handled to avoid anything happening from default
  // processing if it the first click-part was handled by us.
  if (last_click_handler_ && (event.flags() & ui::EF_IS_DOUBLE_CLICK))
    hit_disabled_view = true;

  last_click_handler_ = NULL;
  return hit_disabled_view;
}

bool RootView::OnMouseDragged(const ui::MouseEvent& event) {
  if (mouse_pressed_handler_) {
    SetMouseLocationAndFlags(event);

    ui::MouseEvent mouse_event(event, static_cast<View*>(this),
                               mouse_pressed_handler_);
    return mouse_pressed_handler_->ProcessMouseDragged(mouse_event,
                                                       &drag_info_);
  }
  return false;
}

void RootView::OnMouseReleased(const ui::MouseEvent& event) {
  UpdateCursor(event);

  if (mouse_pressed_handler_) {
    ui::MouseEvent mouse_released(event, static_cast<View*>(this),
                                  mouse_pressed_handler_);
    // We allow the view to delete us from ProcessMouseReleased. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;
    SetMouseHandler(NULL);
    mouse_pressed_handler->ProcessMouseReleased(mouse_released);
    // WARNING: we may have been deleted.
  }
}

void RootView::OnMouseCaptureLost() {
  // TODO: this likely needs to reset touch handler too.

  if (mouse_pressed_handler_ || gesture_handler_) {
    // Synthesize a release event for UpdateCursor.
    if (mouse_pressed_handler_) {
      gfx::Point last_point(last_mouse_event_x_, last_mouse_event_y_);
      ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED,
                                   last_point, last_point,
                                   last_mouse_event_flags_);
      UpdateCursor(release_event);
    }
    // We allow the view to delete us from OnMouseCaptureLost. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;
    View* gesture_handler = gesture_handler_;
    SetMouseHandler(NULL);
    if (mouse_pressed_handler)
      mouse_pressed_handler->OnMouseCaptureLost();
    else
      gesture_handler->OnMouseCaptureLost();
    // WARNING: we may have been deleted.
  }
}

void RootView::OnMouseMoved(const ui::MouseEvent& event) {
  View* v = GetEventHandlerForPoint(event.location());
  // Find the first enabled view, or the existing move handler, whichever comes
  // first.  The check for the existing handler is because if a view becomes
  // disabled while handling moves, it's wrong to suddenly send ET_MOUSE_EXITED
  // and ET_MOUSE_ENTERED events, because the mouse hasn't actually exited yet.
  while (v && !v->enabled() && (v != mouse_move_handler_))
    v = v->parent();
  if (v && v != this) {
    if (v != mouse_move_handler_) {
      if (mouse_move_handler_ != NULL &&
          (!mouse_move_handler_->notify_enter_exit_on_child() ||
           !mouse_move_handler_->Contains(v))) {
        mouse_move_handler_->OnMouseExited(event);
        NotifyEnterExitOfDescendant(event, EVENT_EXIT, mouse_move_handler_, v);
      }
      View* old_handler = mouse_move_handler_;
      mouse_move_handler_ = v;
      ui::MouseEvent entered_event(event, static_cast<View*>(this),
                                   mouse_move_handler_);
      if (!mouse_move_handler_->notify_enter_exit_on_child() ||
          !mouse_move_handler_->Contains(old_handler)) {
        mouse_move_handler_->OnMouseEntered(entered_event);
        NotifyEnterExitOfDescendant(entered_event, EVENT_ENTER, v,
            old_handler);
      }
    }
    ui::MouseEvent moved_event(event, static_cast<View*>(this),
                               mouse_move_handler_);
    mouse_move_handler_->OnMouseMoved(moved_event);
    if (!(moved_event.flags() & ui::EF_IS_NON_CLIENT))
      widget_->SetCursor(mouse_move_handler_->GetCursor(moved_event));
  } else if (mouse_move_handler_ != NULL) {
    mouse_move_handler_->OnMouseExited(event);
    NotifyEnterExitOfDescendant(event, EVENT_EXIT, mouse_move_handler_, v);
    // On Aura the non-client area extends slightly outside the root view for
    // some windows.  Let the non-client cursor handling code set the cursor
    // as we do above.
    if (!(event.flags() & ui::EF_IS_NON_CLIENT))
      widget_->SetCursor(gfx::kNullCursor);
    mouse_move_handler_ = NULL;
  }
}

void RootView::OnMouseExited(const ui::MouseEvent& event) {
  if (mouse_move_handler_ != NULL) {
    mouse_move_handler_->OnMouseExited(event);
    NotifyEnterExitOfDescendant(event, EVENT_EXIT, mouse_move_handler_, NULL);
    mouse_move_handler_ = NULL;
  }
}

bool RootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  bool consumed = false;
  for (View* v = GetFocusManager() ? GetFocusManager()->GetFocusedView() : NULL;
       v && v != this && !consumed; v = v->parent())
    consumed = v->OnMouseWheel(event);
  return consumed;
}

bool RootView::OnScrollEvent(const ui::ScrollEvent& event) {
  bool consumed = false;
  for (View* v = GetEventHandlerForPoint(event.location());
       v && v != this && !consumed; v = v->parent())
    consumed = v->OnScrollEvent(event);
  return consumed;
}

ui::TouchStatus RootView::OnTouchEvent(const ui::TouchEvent& event) {
  // TODO: this looks all wrong. On a TOUCH_PRESSED we should figure out the
  // view and target that view with all touches with the same id until the
  // release (or keep it if captured).

  // If touch_pressed_handler_ is non null, we are currently processing
  // a touch down on the screen situation. In that case we send the
  // event to touch_pressed_handler_
  ui::TouchStatus status = ui::TOUCH_STATUS_UNKNOWN;

  if (touch_pressed_handler_) {
    ui::TouchEvent touch_event(event, static_cast<View*>(this),
                               touch_pressed_handler_);
    status = touch_pressed_handler_->ProcessTouchEvent(touch_event);
    if (status == ui::TOUCH_STATUS_END)
      touch_pressed_handler_ = NULL;
    return status;
  }

  // Walk up the tree until we find a view that wants the touch event.
  for (touch_pressed_handler_ = GetEventHandlerForPoint(event.location());
       touch_pressed_handler_ && (touch_pressed_handler_ != this);
       touch_pressed_handler_ = touch_pressed_handler_->parent()) {
    if (!touch_pressed_handler_->enabled()) {
      // Disabled views eat events but are treated as not handled.
      status = ui::TOUCH_STATUS_UNKNOWN;
      break;
    }

    // See if this view wants to handle the touch
    ui::TouchEvent touch_event(event, static_cast<View*>(this),
                               touch_pressed_handler_);
    status = touch_pressed_handler_->ProcessTouchEvent(touch_event);

    // The view could have removed itself from the tree when handling
    // OnTouchEvent(). So handle as per OnMousePressed. NB: we
    // assume that the RootView itself cannot be so removed.
    if (!touch_pressed_handler_)
      break;

    // The touch event wasn't processed. Go up the view hierarchy and dispatch
    // the touch event.
    if (status == ui::TOUCH_STATUS_UNKNOWN)
      continue;

    // If the touch didn't initiate a touch-sequence, then reset the touch event
    // handler. Otherwise, leave it set so that subsequent touch events are
    // dispatched to the same handler.
    if (status != ui::TOUCH_STATUS_START)
      touch_pressed_handler_ = NULL;

    return status;
  }

  // Reset touch_pressed_handler_ to indicate that no processing is occurring.
  touch_pressed_handler_ = NULL;

  return status;
}

ui::EventResult RootView::OnGestureEvent(const ui::GestureEvent& event) {
  ui::EventResult status = ui::ER_UNHANDLED;

  if (gesture_handler_) {
    // |gesture_handler_| (or |scroll_gesture_handler_|) can be deleted during
    // processing.
    View* handler = scroll_gesture_handler_ &&
        (event.IsScrollGestureEvent() || event.IsFlingScrollEvent())  ?
            scroll_gesture_handler_ : gesture_handler_;
    ui::GestureEvent handler_event(event, static_cast<View*>(this), handler);

    ui::EventResult status = handler->ProcessGestureEvent(handler_event);

    if (event.type() == ui::ET_GESTURE_END &&
        event.details().touch_points() <= 1)
      gesture_handler_ = NULL;

    if (scroll_gesture_handler_ && (event.type() == ui::ET_GESTURE_SCROLL_END ||
                                    event.type() == ui::ET_SCROLL_FLING_START))
      scroll_gesture_handler_ = NULL;

    if (status == ui::ER_CONSUMED)
      return status;

    DCHECK_EQ(ui::ER_UNHANDLED, status);

    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN &&
        !scroll_gesture_handler_) {
      // Some view started processing gesture events, however it does not
      // process scroll-gesture events. In such case, we allow the event to
      // bubble up, and install a different scroll-gesture handler different
      // from the default gesture handler.
      for (scroll_gesture_handler_ = gesture_handler_->parent();
          scroll_gesture_handler_ && scroll_gesture_handler_ != this;
          scroll_gesture_handler_ = scroll_gesture_handler_->parent()) {
        ui::GestureEvent gesture_event(event, static_cast<View*>(this),
                                       scroll_gesture_handler_);
        status = scroll_gesture_handler_->ProcessGestureEvent(gesture_event);
        if (status == ui::ER_CONSUMED)
          return status;
      }
      scroll_gesture_handler_ = NULL;
    }

    return ui::ER_UNHANDLED;
  }

  // Walk up the tree until we find a view that wants the gesture event.
  for (gesture_handler_ = GetEventHandlerForPoint(event.location());
      gesture_handler_ && (gesture_handler_ != this);
      gesture_handler_ = gesture_handler_->parent()) {
    if (!gesture_handler_->enabled()) {
      // Disabled views eat events but are treated as not handled.
      return ui::ER_UNHANDLED;
    }

    // See if this view wants to handle the Gesture.
    ui::GestureEvent gesture_event(event, static_cast<View*>(this),
                                   gesture_handler_);
    status = gesture_handler_->ProcessGestureEvent(gesture_event);

    // The view could have removed itself from the tree when handling
    // OnGestureEvent(). So handle as per OnMousePressed. NB: we
    // assume that the RootView itself cannot be so removed.
    if (!gesture_handler_)
      return ui::ER_UNHANDLED;

    if (status == ui::ER_CONSUMED) {
      if (gesture_event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
        scroll_gesture_handler_ = gesture_handler_;
      return status;
    }

    // The gesture event wasn't processed. Go up the view hierarchy and
    // dispatch the gesture event.
    DCHECK_EQ(ui::ER_UNHANDLED, status);
  }

  gesture_handler_ = NULL;

  return status;
}

void RootView::SetMouseHandler(View* new_mh) {
  // If we're clearing the mouse handler, clear explicit_mouse_handler_ as well.
  explicit_mouse_handler_ = (new_mh != NULL);
  mouse_pressed_handler_ = new_mh;
  gesture_handler_ = new_mh;
  scroll_gesture_handler_ = new_mh;
  drag_info_.Reset();
}

void RootView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_APPLICATION;
}

void RootView::ReorderChildLayers(ui::Layer* parent_layer) {
  View::ReorderChildLayers(parent_layer);
}

////////////////////////////////////////////////////////////////////////////////
// RootView, protected:

void RootView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  widget_->ViewHierarchyChanged(is_add, parent, child);

  if (!is_add) {
    if (!explicit_mouse_handler_ && mouse_pressed_handler_ == child)
      mouse_pressed_handler_ = NULL;
    if (mouse_move_handler_ == child)
      mouse_move_handler_ = NULL;
    if (touch_pressed_handler_ == child)
      touch_pressed_handler_ = NULL;
    if (gesture_handler_ == child)
      gesture_handler_ = NULL;
    if (scroll_gesture_handler_ == child)
      scroll_gesture_handler_ = NULL;
  }
}

void RootView::OnPaint(gfx::Canvas* canvas) {
  if (!layer() || !layer()->fills_bounds_opaquely())
    canvas->DrawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);

  // TODO (pkotwicz): Remove this once we switch over to Aura desktop.
  // This is needed so that we can set the background behind the RWHV when the
  // RWHV is not visible. Not needed once there is a view between the RootView
  // and RWHV.
  View::OnPaint(canvas);
}

void RootView::CalculateOffsetToAncestorWithLayer(gfx::Point* offset,
                                                  ui::Layer** layer_parent) {
  View::CalculateOffsetToAncestorWithLayer(offset, layer_parent);
  if (!layer())
    widget_->CalculateOffsetToAncestorWithLayer(offset, layer_parent);
}

////////////////////////////////////////////////////////////////////////////////
// RootView, private:

// Input -----------------------------------------------------------------------

void RootView::UpdateCursor(const ui::MouseEvent& event) {
  if (!(event.flags() & ui::EF_IS_NON_CLIENT)) {
    View* v = GetEventHandlerForPoint(event.location());
    ui::MouseEvent me(event, static_cast<View*>(this), v);
    widget_->SetCursor(v->GetCursor(me));
  }
}

void RootView::SetMouseLocationAndFlags(const ui::MouseEvent& event) {
  last_mouse_event_flags_ = event.flags();
  last_mouse_event_x_ = event.x();
  last_mouse_event_y_ = event.y();
}

}  // namespace internal
}  // namespace views
