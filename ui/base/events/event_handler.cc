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

EventResult EventHandler::OnEvent(Event* event) {
  if (event->IsKeyEvent())
    return OnKeyEvent(static_cast<KeyEvent*>(event));
  if (event->IsMouseEvent())
    return OnMouseEvent(static_cast<MouseEvent*>(event));
  if (event->IsScrollEvent())
    return OnScrollEvent(static_cast<ScrollEvent*>(event));
  if (event->IsTouchEvent())
    return OnTouchEvent(static_cast<TouchEvent*>(event));
  if (event->IsGestureEvent())
    return OnGestureEvent(static_cast<GestureEvent*>(event));
  return ui::ER_UNHANDLED;
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

EventResult EventHandler::OnGestureEvent(GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

}  // namespace ui
