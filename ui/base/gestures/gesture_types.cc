// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_types.h"

namespace ui {

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         float delta_x,
                                         float delta_y)
    : type_(type) {
  switch (type_) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      data.scroll.x = delta_x;
      data.scroll.y = delta_y;
      break;

    case ui::ET_SCROLL_FLING_START:
      data.velocity.x = delta_x;
      data.velocity.y = delta_y;
      break;

    case ui::ET_GESTURE_TAP:
      data.radius.x = delta_x;
      data.radius.y = delta_y;
      break;

    case ui::ET_GESTURE_LONG_PRESS:
      data.touch_id = static_cast<int>(delta_x);
      CHECK_EQ(0.f, delta_y) << "Unknown data in delta_y for long press.";
      break;

    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
      data.touch_points = static_cast<int>(delta_x);
      CHECK_EQ(0.f, delta_y) << "Unknown data in delta_y for begin/end";
      break;

    case ui::ET_GESTURE_PINCH_UPDATE:
      data.scale = delta_x;
      CHECK_EQ(0.f, delta_y) << "Unknown data in delta_y for pinch";
      break;

    case ui::ET_GESTURE_MULTIFINGER_SWIPE:
      data.swipe.left = delta_x < 0;
      data.swipe.right = delta_x > 0;
      data.swipe.up = delta_y < 0;
      data.swipe.down = delta_y > 0;
      break;

    default:
      data.generic.delta_x = delta_x;
      data.generic.delta_y = delta_y;
      if (delta_x != 0.f || delta_y != 0.f) {
        DLOG(WARNING) << "A gesture event (" << type << ") had unknown data: ("
                      << delta_x << "," << delta_y;
      }
      break;
  }
}

}  // namespace ui
