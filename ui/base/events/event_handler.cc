// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_handler.h"

#include "ui/base/events/event.h"

namespace ui {

EventHandler::EventHandler() {
}

EventHandler::~EventHandler() {
}

void EventHandler::OnEvent(Event* event) {
  ui::EventResult result = ui::ER_UNHANDLED;
  if (event->IsKeyEvent())
    result = OnKeyEvent(static_cast<KeyEvent*>(event));
  else if (event->IsMouseEvent())
    result = OnMouseEvent(static_cast<MouseEvent*>(event));
  else if (event->IsScrollEvent())
    result = OnScrollEvent(static_cast<ScrollEvent*>(event));
  else if (event->IsTouchEvent())
    result = OnTouchEvent(static_cast<TouchEvent*>(event));
  else if (event->IsGestureEvent())
    OnGestureEvent(static_cast<GestureEvent*>(event));

  if (result & ui::ER_CONSUMED)
    event->StopPropagation();
  if (result & ui::ER_HANDLED)
    event->SetHandled();
}

EventResult EventHandler::OnKeyEvent(KeyEvent* event) {
  return ui::ER_UNHANDLED;
}

EventResult EventHandler::OnMouseEvent(MouseEvent* event) {
  return ui::ER_UNHANDLED;
}

EventResult EventHandler::OnScrollEvent(ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

EventResult EventHandler::OnTouchEvent(TouchEvent* event) {
  return ui::ER_UNHANDLED;
}

void EventHandler::OnGestureEvent(GestureEvent* event) {
}

}  // namespace ui
