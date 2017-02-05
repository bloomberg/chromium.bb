// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_event_filter.h"

#include "ui/events/event.h"

namespace keyboard {

void KeyboardEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_END:
    case ui::ET_GESTURE_PINCH_UPDATE:
      event->StopPropagation();
      break;
    default:
      break;
  }
}

}  // nemespace keyboard
