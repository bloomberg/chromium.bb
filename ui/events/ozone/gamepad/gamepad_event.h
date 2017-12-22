// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_
#define UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_

#include "base/time/time.h"
#include "ui/events/ozone/gamepad/webgamepad_constants.h"

namespace ui {

struct GamepadEvent {
  GamepadEvent(int device_id,
               GamepadEventType type,
               uint16_t code,
               double value,
               base::TimeTicks timestamp)
      : device_id(device_id),
        type(type),
        code(code),
        value(value),
        timestamp(timestamp) {}

  int device_id;

  GamepadEventType type;

  uint16_t code;

  double value;

  base::TimeTicks timestamp;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_
