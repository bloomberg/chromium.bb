// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

#include "ui/aura/window.h"
#include "ui/base/events/event.h"

namespace aura {

bool EventFilter::PreHandleKeyEvent(Window* target, ui::KeyEvent* event) {
  return false;
}

bool EventFilter::PreHandleMouseEvent(Window* target, ui::MouseEvent* event) {
  return false;
}

ui::TouchStatus EventFilter::PreHandleTouchEvent(Window* target,
                                                 ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult EventFilter::PreHandleGestureEvent(Window* target,
                                                   ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::EventResult EventFilter::OnKeyEvent(ui::KeyEvent* event) {
  return PreHandleKeyEvent(static_cast<Window*>(event->target()), event) ?
      ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

ui::EventResult EventFilter::OnMouseEvent(ui::MouseEvent* event) {
  return PreHandleMouseEvent(static_cast<Window*>(event->target()), event) ?
      ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

ui::EventResult EventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::TouchStatus EventFilter::OnTouchEvent(ui::TouchEvent* event) {
  return PreHandleTouchEvent(static_cast<Window*>(event->target()), event);
}

ui::EventResult EventFilter::OnGestureEvent(ui::GestureEvent* event) {
  return PreHandleGestureEvent(static_cast<Window*>(event->target()), event);
}

}  // namespace aura
