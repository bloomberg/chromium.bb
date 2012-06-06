// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_COMPOUND_EVENT_FILTER_H_
#define UI_AURA_SHARED_COMPOUND_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"

namespace aura {
class CursorManager;
class RootWindow;

namespace shared {

// CompoundEventFilter gets all events first and can provide actions to those
// events. It implements global features such as click to activate a window and
// cursor change when moving mouse.
// Additional event filters can be added to CompoundEventFilter. Events will
// pass through those additional filters in their addition order and could be
// consumed by any of those filters. If an event is consumed by a filter, the
// rest of the filter(s) and CompoundEventFilter will not see the consumed
// event.
class AURA_EXPORT CompoundEventFilter : public aura::EventFilter {
 public:
  CompoundEventFilter();
  virtual ~CompoundEventFilter();

  // Returns the cursor for the specified component.
  static gfx::NativeCursor CursorForWindowComponent(int window_component);

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

  // Dispatches event to additional filters. Returns false or
  // ui::TOUCH_STATUS_UNKNOWN if event is consumed.
  bool FilterKeyEvent(aura::Window* target, aura::KeyEvent* event);
  bool FilterMouseEvent(aura::Window* target, aura::MouseEvent* event);
  ui::TouchStatus FilterTouchEvent(aura::Window* target,
                                   aura::TouchEvent* event);

  // Sets the visibility of the cursor if the event is not synthesized and
  // |update_cursor_visibility_| is true.
  void SetVisibilityOnEvent(aura::LocatedEvent* event, bool show);

  // Additional event filters that pre-handles events.
  ObserverList<aura::EventFilter, true> filters_;

  // Should we show the mouse cursor when we see mouse movement and hide it when
  // we see a touch event?
  bool update_cursor_visibility_;

  DISALLOW_COPY_AND_ASSIGN(CompoundEventFilter);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_COMPOUND_EVENT_FILTER_H_
