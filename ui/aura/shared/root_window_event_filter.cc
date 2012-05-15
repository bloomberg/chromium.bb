// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/root_window_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"

namespace aura {
namespace shared {

namespace {

aura::Window* FindFocusableWindowFor(aura::Window* window) {
  while (window && !window->CanFocus())
    window = window->parent();
  return window;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RootWindowEventFilter, public:

RootWindowEventFilter::RootWindowEventFilter(aura::RootWindow* root_window)
    : root_window_(root_window),
      cursor_lock_count_(0),
      did_cursor_change_(false),
      cursor_to_set_on_unlock_(0),
      update_cursor_visibility_(true) {
}

RootWindowEventFilter::~RootWindowEventFilter() {
  // Additional filters are not owned by RootWindowEventFilter and they
  // should all be removed when running here. |filters_| has
  // check_empty == true and will DCHECK failure if it is not empty.
}

// static
gfx::NativeCursor RootWindowEventFilter::CursorForWindowComponent(
    int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ui::kCursorSouthResize;
    case HTBOTTOMLEFT:
      return ui::kCursorSouthWestResize;
    case HTBOTTOMRIGHT:
      return ui::kCursorSouthEastResize;
    case HTLEFT:
      return ui::kCursorWestResize;
    case HTRIGHT:
      return ui::kCursorEastResize;
    case HTTOP:
      return ui::kCursorNorthResize;
    case HTTOPLEFT:
      return ui::kCursorNorthWestResize;
    case HTTOPRIGHT:
      return ui::kCursorNorthEastResize;
    default:
      return ui::kCursorNull;
  }
}

void RootWindowEventFilter::LockCursor() {
  cursor_lock_count_++;
}

void RootWindowEventFilter::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ == 0) {
    if (did_cursor_change_) {
      did_cursor_change_ = false;
      root_window_->SetCursor(cursor_to_set_on_unlock_);
    }
    did_cursor_change_ = false;
    cursor_to_set_on_unlock_ = 0;
  }
}

void RootWindowEventFilter::AddFilter(aura::EventFilter* filter) {
  filters_.AddObserver(filter);
}

void RootWindowEventFilter::RemoveFilter(aura::EventFilter* filter) {
  filters_.RemoveObserver(filter);
}

size_t RootWindowEventFilter::GetFilterCount() const {
  return filters_.size();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowEventFilter, EventFilter implementation:

bool RootWindowEventFilter::PreHandleKeyEvent(aura::Window* target,
                                              aura::KeyEvent* event) {
  return FilterKeyEvent(target, event);
}

bool RootWindowEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                aura::MouseEvent* event) {
  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  // It should also update the cursor for clicking and wheels for ChromeOS boot.
  // When ChromeOS is booted, it hides the mouse cursor but immediate mouse
  // operation will show the cursor.
  if (event->type() == ui::ET_MOUSE_MOVED ||
      event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSEWHEEL) {
    if (update_cursor_visibility_)
      SetCursorVisible(target, event, true);

    UpdateCursor(target, event);
  }

  if (FilterMouseEvent(target, event))
    return true;

  if (event->type() == ui::ET_MOUSE_PRESSED && GetActiveWindow() != target)
    target->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(target), event);

  return false;
}

ui::TouchStatus RootWindowEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  ui::TouchStatus status = FilterTouchEvent(target, event);
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return status;

  if (event->type() == ui::ET_TOUCH_PRESSED) {
    if (update_cursor_visibility_)
      SetCursorVisible(target, event, false);

    target->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(target), event);
  }
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus RootWindowEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  ui::GestureStatus status = ui::GESTURE_STATUS_UNKNOWN;
  if (filters_.might_have_observers()) {
    ObserverListBase<aura::EventFilter>::Iterator it(filters_);
    aura::EventFilter* filter;
    while (status == ui::GESTURE_STATUS_UNKNOWN &&
        (filter = it.GetNext()) != NULL) {
      status = filter->PreHandleGestureEvent(target, event);
    }
  }
  return status;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowEventFilter, private:

void RootWindowEventFilter::UpdateCursor(aura::Window* target,
                                         aura::MouseEvent* event) {
  gfx::NativeCursor cursor = target->GetCursor(event->location());
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    int window_component =
        target->delegate()->GetNonClientComponent(event->location());
    cursor = CursorForWindowComponent(window_component);
  }
  if (cursor_lock_count_ == 0) {
    root_window_->SetCursor(cursor);
  } else {
    cursor_to_set_on_unlock_ = cursor;
    did_cursor_change_ = true;
  }
}

void RootWindowEventFilter::SetCursorVisible(aura::Window* target,
                                             aura::LocatedEvent* event,
                                             bool show) {
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED))
    root_window_->ShowCursor(show);
}

bool RootWindowEventFilter::FilterKeyEvent(aura::Window* target,
                                           aura::KeyEvent* event) {
  bool handled = false;
  if (filters_.might_have_observers()) {
    ObserverListBase<aura::EventFilter>::Iterator it(filters_);
    aura::EventFilter* filter;
    while (!handled && (filter = it.GetNext()) != NULL)
      handled = filter->PreHandleKeyEvent(target, event);
  }
  return handled;
}

bool RootWindowEventFilter::FilterMouseEvent(aura::Window* target,
                                             aura::MouseEvent* event) {
  bool handled = false;
  if (filters_.might_have_observers()) {
    ObserverListBase<aura::EventFilter>::Iterator it(filters_);
    aura::EventFilter* filter;
    while (!handled && (filter = it.GetNext()) != NULL)
      handled = filter->PreHandleMouseEvent(target, event);
  }
  return handled;
}

ui::TouchStatus RootWindowEventFilter::FilterTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  ui::TouchStatus status = ui::TOUCH_STATUS_UNKNOWN;
  if (filters_.might_have_observers()) {
    ObserverListBase<aura::EventFilter>::Iterator it(filters_);
    aura::EventFilter* filter;
    while (status == ui::TOUCH_STATUS_UNKNOWN &&
        (filter = it.GetNext()) != NULL) {
      status = filter->PreHandleTouchEvent(target, event);
    }
  }
  return status;
}

Window* RootWindowEventFilter::GetActiveWindow() {
  return aura::client::GetActivationClient(root_window_)->
      GetActiveWindow();
}

}  // namespace shared
}  // namespace aura
