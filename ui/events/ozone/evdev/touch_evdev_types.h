// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_

#include "base/basictypes.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

// Number of supported touch slots. ABS_MT_SLOT messages with
// value >= kNumTouchEvdevSlots are ignored.
const int kNumTouchEvdevSlots = 20;

// Contains information about an in progress touch.
struct EVENTS_OZONE_EVDEV_EXPORT InProgressTouchEvdev {
  InProgressTouchEvdev();
  ~InProgressTouchEvdev();

  // Whether there is new information for the touch.
  bool altered;

  // Whether the touch was cancelled. Touch events should be ignored till a
  // new touch is initiated.
  bool cancelled;

  bool was_touching;
  bool touching;
  float x;
  float y;
  int tracking_id;
  size_t slot;
  float radius_x;
  float radius_y;
  float pressure;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_
