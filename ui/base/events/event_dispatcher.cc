// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_dispatcher.h"

namespace ui {

EventDispatcher::EventDispatcher()
    : set_on_destroy_(NULL),
      current_event_(NULL) {
}

EventDispatcher::~EventDispatcher() {
  if (set_on_destroy_)
    *set_on_destroy_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher, private:

EventResult EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                          Event* event) {
  return handler->OnEvent(event);
}

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
  return handler->OnTouchEvent(event);
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
