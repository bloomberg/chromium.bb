// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_HANDLER_H_
#define UI_BASE_EVENTS_EVENT_HANDLER_H_

#include <vector>

#include "ui/base/events/event_constants.h"
#include "ui/base/ui_export.h"

namespace ui {

class GestureEvent;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;

class EventTarget;

// Dispatches events to appropriate targets.
class UI_EXPORT EventHandler {
 public:
  EventHandler() {}
  virtual ~EventHandler() {}

  virtual EventResult OnKeyEvent(KeyEvent* event) = 0;

  virtual EventResult OnMouseEvent(MouseEvent* event) = 0;

  virtual EventResult OnScrollEvent(ScrollEvent* event) = 0;

  virtual TouchStatus OnTouchEvent(TouchEvent* event) = 0;

  virtual EventResult OnGestureEvent(GestureEvent* event) = 0;
};

typedef std::vector<EventHandler*> EventHandlerList;

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_HANDLER_H_
