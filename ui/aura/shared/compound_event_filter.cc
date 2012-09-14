// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/compound_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/events/event.h"
#include "ui/base/hit_test.h"

namespace aura {
namespace shared {

namespace {

Window* FindFocusableWindowFor(Window* window) {
  while (window && !window->CanFocus())
    window = window->parent();
  return window;
}

Window* GetActiveWindow(Window* window) {
  DCHECK(window->GetRootWindow());
  return client::GetActivationClient(window->GetRootWindow())->
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

void CompoundEventFilter::AddFilter(EventFilter* filter) {
  filters_.AddObserver(filter);
}

void CompoundEventFilter::RemoveFilter(EventFilter* filter) {
  filters_.RemoveObserver(filter);
}

size_t CompoundEventFilter::GetFilterCount() const {
  return filters_.size();
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, private:

void CompoundEventFilter::UpdateCursor(Window* target, ui::MouseEvent* event) {
  // If drag and drop is in progress, let the drag drop client set the cursor
  // instead of setting the cursor here.
  aura::RootWindow* root_window = target->GetRootWindow();
  client::DragDropClient* drag_drop_client =
      client::GetDragDropClient(root_window);
  if (drag_drop_client && drag_drop_client->IsDragDropInProgress())
    return;

  client::CursorClient* cursor_client =
      client::GetCursorClient(root_window);
  if (cursor_client) {
    gfx::NativeCursor cursor = target->GetCursor(event->location());
    if (event->flags() & ui::EF_IS_NON_CLIENT) {
      int window_component =
          target->delegate()->GetNonClientComponent(event->location());
      cursor = CursorForWindowComponent(window_component);
    }

    cursor_client->SetCursor(cursor);
    cursor_client->SetDeviceScaleFactor(
        root_window->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
  }
}

ui::EventResult CompoundEventFilter::FilterKeyEvent(ui::KeyEvent* event) {
  int result = ui::ER_UNHANDLED;
  if (filters_.might_have_observers()) {
    ObserverListBase<EventFilter>::Iterator it(filters_);
    EventFilter* filter;
    while (!(result & ui::ER_CONSUMED) && (filter = it.GetNext()) != NULL)
      result |= filter->OnKeyEvent(event);
  }
  return static_cast<ui::EventResult>(result);
}

ui::EventResult CompoundEventFilter::FilterMouseEvent(ui::MouseEvent* event) {
  int result = ui::ER_UNHANDLED;
  if (filters_.might_have_observers()) {
    ObserverListBase<EventFilter>::Iterator it(filters_);
    EventFilter* filter;
    while (!(result & ui::ER_CONSUMED) && (filter = it.GetNext()) != NULL)
      result |= filter->OnMouseEvent(event);
  }
  return static_cast<ui::EventResult>(result);
}

ui::TouchStatus CompoundEventFilter::FilterTouchEvent(
    Window* target,
    ui::TouchEvent* event) {
  ui::TouchStatus status = ui::TOUCH_STATUS_UNKNOWN;
  if (filters_.might_have_observers()) {
    ObserverListBase<EventFilter>::Iterator it(filters_);
    EventFilter* filter;
    while (status == ui::TOUCH_STATUS_UNKNOWN &&
        (filter = it.GetNext()) != NULL) {
      status = filter->OnTouchEvent(event);
    }
  }
  return status;
}

void CompoundEventFilter::SetCursorVisibilityOnEvent(aura::Window* target,
                                                     ui::Event* event,
                                                     bool show) {
  if (update_cursor_visibility_ && !(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    client::CursorClient* client =
        client::GetCursorClient(target->GetRootWindow());
    if (client)
      client->ShowCursor(show);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, EventFilter implementation:

ui::TouchStatus CompoundEventFilter::PreHandleTouchEvent(
    Window* target,
    ui::TouchEvent* event) {
  // TODO(sad): Move the implementation into OnTouchEvent once touch-events are
  // hooked up to go through EventDispatcher.
  ui::TouchStatus status = FilterTouchEvent(target, event);
  if (status == ui::TOUCH_STATUS_UNKNOWN &&
      event->type() == ui::ET_TOUCH_PRESSED) {
    SetCursorVisibilityOnEvent(target, event, false);
  }
  return status;
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, ui::EventHandler implementation:

ui::EventResult CompoundEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  SetCursorVisibilityOnEvent(
      static_cast<Window*>(event->target()), event, false);
  return FilterKeyEvent(event);
}

ui::EventResult CompoundEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  Window* window = static_cast<Window*>(event->target());
  WindowTracker window_tracker;
  window_tracker.Add(window);

  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  // It should also update the cursor for clicking and wheels for ChromeOS boot.
  // When ChromeOS is booted, it hides the mouse cursor but immediate mouse
  // operation will show the cursor.
  // We also update the cursor for mouse enter in case a mouse cursor is sent to
  // outside of the root window and moved back for some reasons (e.g. running on
  // on Desktop for testing, or a bug in pointer barrier).
  if (event->type() == ui::ET_MOUSE_ENTERED ||
      event->type() == ui::ET_MOUSE_MOVED ||
      event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSEWHEEL) {
    SetCursorVisibilityOnEvent(window, event, true);
    UpdateCursor(window, event);
  }

  ui::EventResult result = FilterMouseEvent(event);
  if ((result & ui::ER_CONSUMED) ||
      !window_tracker.Contains(window) ||
      !window->GetRootWindow()) {
    return result;
  }

  if (event->type() == ui::ET_MOUSE_PRESSED &&
      GetActiveWindow(window) != window) {
    window->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(window), event);
  }

  return result;
}

ui::EventResult CompoundEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::TouchStatus CompoundEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  return EventFilter::OnTouchEvent(event);
}

ui::EventResult CompoundEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  int result = ui::ER_UNHANDLED;
  if (filters_.might_have_observers()) {
    ObserverListBase<EventFilter>::Iterator it(filters_);
    EventFilter* filter;
    while (!(result & ui::ER_CONSUMED) && (filter = it.GetNext()) != NULL)
      result |= filter->OnGestureEvent(event);
  }

  Window* window = static_cast<Window*>(event->target());
  if (event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1 &&
      !(result & ui::ER_CONSUMED) &&
      window->GetRootWindow() &&
      GetActiveWindow(window) != window) {
    window->GetFocusManager()->SetFocusedWindow(
        FindFocusableWindowFor(window), event);
  }

  return static_cast<ui::EventResult>(result);
}

}  // namespace shared
}  // namespace aura
