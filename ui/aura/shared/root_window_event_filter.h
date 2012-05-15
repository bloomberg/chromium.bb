// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_ROOT_WINDOW_EVENT_FILTER_H_
#define UI_AURA_SHARED_ROOT_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"

namespace aura {
class RootWindow;

namespace shared {

// RootWindowEventFilter gets all root window events first and can provide
// actions to those events. It implements root window features such as click to
// activate a window and cursor change when moving mouse.
// Additional event filters can be added to RootWindowEventFilter. Events will
// pass through those additional filters in their addition order and could be
// consumed by any of those filters. If an event is consumed by a filter, the
// rest of the filter(s) and RootWindowEventFilter will not see the consumed
// event.
class AURA_EXPORT RootWindowEventFilter : public aura::EventFilter {
 public:
  RootWindowEventFilter(aura::RootWindow* root_window);
  virtual ~RootWindowEventFilter();

  // Returns the cursor for the specified component.
  static gfx::NativeCursor CursorForWindowComponent(int window_component);

  // Freezes updates to the cursor until UnlockCursor() is invoked.
  void LockCursor();

  // Unlocks the cursor.
  void UnlockCursor();

  void set_update_cursor_visibility(bool update) {
    update_cursor_visibility_ = update;
  }

  // Adds/removes additional event filters. This does not take ownership of
  // the EventFilter.
  void AddFilter(aura::EventFilter* filter);
  void RemoveFilter(aura::EventFilter* filter);
  size_t GetFilterCount() const;

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void UpdateCursor(aura::Window* target, aura::MouseEvent* event);

  // Sets the cursor invisible when the target receives touch press event.
  void SetCursorVisible(aura::Window* target,
                        aura::LocatedEvent* event,
                        bool show);

  // Dispatches event to additional filters. Returns false or
  // ui::TOUCH_STATUS_UNKNOWN if event is consumed.
  bool FilterKeyEvent(aura::Window* target, aura::KeyEvent* event);
  bool FilterMouseEvent(aura::Window* target, aura::MouseEvent* event);
  ui::TouchStatus FilterTouchEvent(aura::Window* target,
                                   aura::TouchEvent* event);

  // Gets the active window from the activation client.
  aura::Window* GetActiveWindow();

  aura::RootWindow* root_window_;

  // Additional event filters that pre-handles events.
  ObserverList<aura::EventFilter, true> filters_;

  // Number of times LockCursor() has been invoked without a corresponding
  // UnlockCursor().
  int cursor_lock_count_;

  // Set to true if UpdateCursor() is invoked while |cursor_lock_count_| == 0.
  bool did_cursor_change_;

  // Cursor to set once |cursor_lock_count_| is set to 0. Only valid if
  // |did_cursor_change_| is true.
  gfx::NativeCursor cursor_to_set_on_unlock_;

  // Should we show the mouse cursor when we see mouse movement and hide it when
  // we see a touch event?
  bool update_cursor_visibility_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowEventFilter);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_EVENT_FILTER_H_
