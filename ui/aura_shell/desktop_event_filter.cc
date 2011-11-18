// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_event_filter.h"

#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_accelerator_filter.h"
#include "ui/aura_shell/stacking_controller.h"
#include "ui/base/hit_test.h"

namespace aura_shell {
namespace internal {

// Returns the default cursor for a window component.
gfx::NativeCursor CursorForWindowComponent(int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return aura::kCursorSouthResize;
    case HTBOTTOMLEFT:
      return aura::kCursorSouthWestResize;
    case HTBOTTOMRIGHT:
      return aura::kCursorSouthEastResize;
    case HTLEFT:
      return aura::kCursorWestResize;
    case HTRIGHT:
      return aura::kCursorEastResize;
    case HTTOP:
      return aura::kCursorNorthResize;
    case HTTOPLEFT:
      return aura::kCursorNorthWestResize;
    case HTTOPRIGHT:
      return aura::kCursorNorthEastResize;
    default:
      return aura::kCursorNull;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopEventFilter, public:

DesktopEventFilter::DesktopEventFilter()
    : EventFilter(aura::Desktop::GetInstance()),
      shell_accelerator_filter_(new ShellAcceleratorFilter) {
  AddFilter(shell_accelerator_filter_.get());
}

DesktopEventFilter::~DesktopEventFilter() {
  // Additional filters are not owned by DesktopEventFilter and they
  // should all be removed when running here. |filters_| has
  // check_empty == true and will DCHECK failure if it is not empty.
  RemoveFilter(shell_accelerator_filter_.get());
}

void DesktopEventFilter::AddFilter(aura::EventFilter* filter) {
  filters_.AddObserver(filter);
}

void DesktopEventFilter::RemoveFilter(aura::EventFilter* filter) {
  filters_.RemoveObserver(filter);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopEventFilter, EventFilter implementation:

bool DesktopEventFilter::PreHandleKeyEvent(aura::Window* target,
                                           aura::KeyEvent* event) {
  return FilterKeyEvent(target, event);
}

bool DesktopEventFilter::PreHandleMouseEvent(aura::Window* target,
                                             aura::MouseEvent* event) {
  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  if (event->type() == ui::ET_MOUSE_MOVED)
    UpdateCursor(target, event);

  if (FilterMouseEvent(target, event))
    return true;

  if (event->type() == ui::ET_MOUSE_PRESSED)
    ActivateIfNecessary(target, event);

  return false;
}

ui::TouchStatus DesktopEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  ui::TouchStatus status = FilterTouchEvent(target, event);
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return status;

  if (event->type() == ui::ET_TOUCH_PRESSED)
    ActivateIfNecessary(target, event);
  return ui::TOUCH_STATUS_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopEventFilter, private:

void DesktopEventFilter::ActivateIfNecessary(aura::Window* window,
                                             aura::Event* event) {
  aura::Window* activatable = StackingController::GetActivatableWindow(window);
  if (activatable == aura::Desktop::GetInstance()->active_window()) {
    // |window| is a descendant of the active window, no need to activate.
    window->GetFocusManager()->SetFocusedWindow(window);
  } else {
    aura::Desktop::GetInstance()->SetActiveWindow(activatable, window);
  }
}

void DesktopEventFilter::UpdateCursor(aura::Window* target,
                                      aura::MouseEvent* event) {
  gfx::NativeCursor cursor = target->GetCursor(event->location());
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    int window_component =
        target->delegate()->GetNonClientComponent(event->location());
    cursor = CursorForWindowComponent(window_component);
  }
  aura::Desktop::GetInstance()->SetCursor(cursor);
}

bool DesktopEventFilter::FilterKeyEvent(aura::Window* target,
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

bool DesktopEventFilter::FilterMouseEvent(aura::Window* target,
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

ui::TouchStatus DesktopEventFilter::FilterTouchEvent(aura::Window* target,
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

}  // namespace internal
}  // namespace aura_shell
