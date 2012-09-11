// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_dispatcher.h"

namespace ui {

EventDispatcher::EventDispatcher() {
}

EventDispatcher::~EventDispatcher() {
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher, private:

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          KeyEvent* event) {
  return handler->OnKeyEvent(event);
}

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          MouseEvent* event) {
  return handler->OnMouseEvent(event);
}

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          ScrollEvent* event) {
  return handler->OnScrollEvent(event);
}

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          TouchEvent* event) {
  // TODO(sad): This needs fixing (especially for the QUEUED_ status).
  TouchStatus status = handler->OnTouchEvent(event);
  return status == ui::TOUCH_STATUS_UNKNOWN ? ER_UNHANDLED :
         status == ui::TOUCH_STATUS_QUEUED_END ? ER_CONSUMED :
                                                 ER_HANDLED;
}

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          GestureEvent* event) {
  return handler->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher::ScopedDispatchHelper

EventDispatcher::ScopedDispatchHelper::ScopedDispatchHelper(Event* event)
    : Event::DispatcherApi(event) {
}

EventDispatcher::ScopedDispatchHelper::~ScopedDispatchHelper() {
  set_phase(EP_POSTDISPATCH);
}

}  // namespace ui
