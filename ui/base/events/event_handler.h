// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_HANDLER_H_
#define UI_BASE_EVENTS_EVENT_HANDLER_H_

#include <vector>

#include "ui/base/events.h"
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

  virtual EventResult OnKeyEvent(EventTarget* target,
                                 KeyEvent* event) = 0;

  virtual EventResult OnMouseEvent(EventTarget* target,
                                   MouseEvent* event) = 0;

  virtual EventResult OnScrollEvent(EventTarget* target,
                                    ScrollEvent* event) = 0;

  virtual TouchStatus OnTouchEvent(EventTarget* target,
                                   TouchEvent* event) = 0;

  virtual EventResult OnGestureEvent(EventTarget* target,
                                     GestureEvent* event) = 0;
};

typedef std::vector<EventHandler*> EventHandlerList;

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_HANDLER_H_
