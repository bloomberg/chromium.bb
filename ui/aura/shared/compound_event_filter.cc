// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/compound_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/cursor_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"

namespace aura {
namespace shared {

namespace {

aura::Window* FindFocusableWindowFor(aura::Window* window) {
  while (window && !window->CanFocus())
    window = window->parent();
  return window;
}

aura::Window* GetActiveWindow(aura::Window* window) {
  DCHECK(window->GetRootWindow());
  return aura::client::GetActivationClient(window->GetRootWindow())->
      GetActiveWindow();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, public:

CompoundEventFilter::CompoundEventFilter() : update_cursor_visibility_(true) {
}

CompoundEventFilter::~CompoundEventFilter() {
  // Additional filters are not owned by CompoundEventFilter and they
  // should all be removed when running here. |filters_| has
  // check_empty == true and will DCHECK failure if it is not empty.
}

// static
gfx::NativeCursor CompoundEventFilter::CursorForWindowComponent(
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

void CompoundEventFilter::AddFilter(aura::EventFilter* filter) {
  filters_.AddObserver(filter);
}

void CompoundEventFilter::RemoveFilter(aura::EventFilter* filter) {
  filters_.RemoveObserver(filter);
}

size_t CompoundEventFilter::GetFilterCount() const {
  return filters_.size();
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, EventFilter implementation:

bool CompoundEventFilter::PreHandleKeyEvent(aura::Window* target,
                                              aura::KeyEvent* event) {
  return FilterKeyEvent(target, event);
}

bool CompoundEventFilter::PreHandleMouseEvent(aura::Window* target,
                                              aura::MouseEvent* event) {
  WindowTracker window_tracker;
  window_tracker.Add(target);

  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  // It should also update the cursor for clicking and wheels for ChromeOS boot.
  // When ChromeOS is booted, it hides the mouse cursor but immediate mouse
  // operation will show the cursor.
  if (event->type() == ui::ET_MOUSE_MOVED ||
      event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSEWHEEL) {
    SetVisibilityOnEvent(event, true);
    UpdateCursor(target, event);
  }

  if (FilterMouseEvent(target, event) ||
      !window_tracker.Contains(target) ||
      !target->GetRootWindow()) {
    return true;
  }

  if (event->type() == ui::ET_MOUSE_PRESSED &&
      GetActiveWindow(target) != target) {
    target->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(target), event);
  }

  return false;
}

ui::TouchStatus CompoundEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  ui::TouchStatus status = FilterTouchEvent(target, event);
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return status;

  if (event->type() == ui::ET_TOUCH_PRESSED) {
    SetVisibilityOnEvent(event, false);
    target->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(target), event);
  }

  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus CompoundEventFilter::PreHandleGestureEvent(
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
// CompoundEventFilter, private:

void CompoundEventFilter::UpdateCursor(aura::Window* target,
                                  aura::MouseEvent* event) {
  gfx::NativeCursor cursor = target->GetCursor(event->location());
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    int window_component =
        target->delegate()->GetNonClientComponent(event->location());
    cursor = CursorForWindowComponent(window_component);
  }
  Env::GetInstance()->cursor_manager()->SetCursor(cursor);
}

bool CompoundEventFilter::FilterKeyEvent(aura::Window* target,
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

bool CompoundEventFilter::FilterMouseEvent(aura::Window* target,
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

ui::TouchStatus CompoundEventFilter::FilterTouchEvent(
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

void CompoundEventFilter::SetVisibilityOnEvent(aura::LocatedEvent* event,
                                          bool show) {
  if (update_cursor_visibility_ && !(event->flags() & ui::EF_IS_SYNTHESIZED))
    Env::GetInstance()->cursor_manager()->ShowCursor(show);
}

}  // namespace shared
}  // namespace aura
