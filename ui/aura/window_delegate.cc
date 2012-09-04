// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_delegate.h"

namespace aura {

ui::EventResult WindowDelegate::OnKeyEvent(ui::EventTarget* target,
                                           ui::KeyEvent* event) {
  return OnKeyEvent(event) ? ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

ui::EventResult WindowDelegate::OnMouseEvent(ui::EventTarget* target,
                                             ui::MouseEvent* event) {
  return OnMouseEvent(event) ? ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

ui::EventResult WindowDelegate::OnScrollEvent(ui::EventTarget* target,
                                              ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::TouchStatus WindowDelegate::OnTouchEvent(ui::EventTarget* target,
                                             ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult WindowDelegate::OnGestureEvent(ui::EventTarget* target,
                                               ui::GestureEvent* event) {
  return OnGestureEvent(event);
}

}  // namespace aura
