// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHARED_COMPOUND_EVENT_FILTER_H_
#define UI_AURA_SHARED_COMPOUND_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/event_filter.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class TouchEvent;
}

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
class AURA_EXPORT CompoundEventFilter : public EventFilter {
 public:
  CompoundEventFilter();
  virtual ~CompoundEventFilter();

  // Returns the cursor for the specified component.
  static gfx::NativeCursor CursorForWindowComponent(int window_component);

  // Adds/removes additional event filters. This does not take ownership of
  // the EventFilter.
  // NOTE: EventFilters are deprecated. Use env::AddPreTargetEventHandler etc.
  // instead.
  void AddFilter(EventFilter* filter);
  void RemoveFilter(EventFilter* filter);

 private:
  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void UpdateCursor(Window* target, ui::MouseEvent* event);

  // Dispatches event to additional filters.
  ui::EventResult FilterKeyEvent(ui::KeyEvent* event);
  ui::EventResult FilterMouseEvent(ui::MouseEvent* event);
  ui::EventResult FilterTouchEvent(ui::TouchEvent* event);

  // Sets the visibility of the cursor if the event is not synthesized and
  // 1) it's hiding (show=false) when the cursor is currently shown, or
  // 2) it's showing (show=true) if the cursor is previously hidden
  //    by this event filter (see |cursor_hidden_by_filter_|),
  // so that it doesn't change the cursor visibility if the cursor was
  // intentionally hidden by other components.
  void SetCursorVisibilityOnEvent(aura::Window* target,
                                  ui::Event* event,
                                  bool show);

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::EventResult OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  // Additional event filters that pre-handles events.
  ObserverList<EventFilter, true> filters_;

  // True if the cursur was hidden by the filter.
  bool cursor_hidden_by_filter_;

  DISALLOW_COPY_AND_ASSIGN(CompoundEventFilter);
};

}  // namespace shared
}  // namespace aura

#endif  // UI_AURA_COMPOUND_EVENT_FILTER_H_
