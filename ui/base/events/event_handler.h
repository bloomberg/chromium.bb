// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_HANDLER_H_
#define UI_BASE_EVENTS_EVENT_HANDLER_H_

#include <vector>

#include "ui/base/events/event_constants.h"
#include "ui/base/ui_export.h"

namespace ui {

class Event;
class GestureEvent;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;

class EventTarget;

// Dispatches events to appropriate targets. The default implementations return
// ER_UNHANDLED for all events.
class UI_EXPORT EventHandler {
 public:
  EventHandler();
  virtual ~EventHandler();

  // This is called for all events. The default implementation routes the event
  // to one of the event-specific callbacks (OnKeyEvent, OnMouseEvent etc.). If
  // this is overridden, then normally, the override should chain into the
  // default implementation for un-handled events.
  virtual void OnEvent(Event* event);

  virtual EventResult OnKeyEvent(KeyEvent* event);

  virtual EventResult OnMouseEvent(MouseEvent* event);

  virtual EventResult OnScrollEvent(ScrollEvent* event);

  virtual EventResult OnTouchEvent(TouchEvent* event);

  virtual void OnGestureEvent(GestureEvent* event);
};

typedef std::vector<EventHandler*> EventHandlerList;

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_HANDLER_H_
