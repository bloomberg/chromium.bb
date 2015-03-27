// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_evdev_types.h"

namespace ui {

InProgressTouchEvdev::InProgressTouchEvdev()
    : altered(false),
      cancelled(false),
      was_touching(false),
      touching(false),
      x(0),
      y(0),
      tracking_id(-1),
      slot(0),
      radius_x(0),
      radius_y(0),
      pressure(0) {
}

InProgressTouchEvdev::~InProgressTouchEvdev() {}

}  // namespace ui
