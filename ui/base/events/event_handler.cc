// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_handler.h"

namespace ui {

EventHandler::EventHandler() {
}

EventHandler::~EventHandler() {
}

EventResult EventHandler::OnEvent(Event* event) {
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
