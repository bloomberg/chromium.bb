// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_
#define UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_

#include "base/time/time.h"
#include "ui/events/ozone/gamepad/webgamepad_constants.h"

namespace ui {

class GamepadEvent {
 public:
  GamepadEvent(int device_id,
               GamepadEventType type,
               uint16_t code,
               uint16_t raw_code,
               double value,
               base::TimeTicks timestamp);

  int device_id() const { return device_id_; }

  GamepadEventType type() const { return type_; }

  uint16_t code() const { return code_; }

  uint16_t raw_code() const { return raw_code_; }

  double value() const { return value_; }

  base::TimeTicks timestamp() const { return timestamp_; }

 private:
  int device_id_;

  GamepadEventType type_;

  // Web Gamepad keycode.
  uint16_t code_;

  // Evdev scancode.
  uint16_t raw_code_;

  double value_;

  base::TimeTicks timestamp_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_EVENT_H_
