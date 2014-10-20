// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_

#include <bitset>
#include <linux/input.h>

#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

class EventModifiersEvdev;

// Keyboard for evdev.
//
// This object is responsible for combining all attached keyboards into
// one logical keyboard, applying modifiers & implementing key repeat.
//
// It also currently also applies the layout (hardcoded as US).
//
// TODO(spang): Implement key repeat & turn off kernel repeat.
class EVENTS_OZONE_EVDEV_EXPORT KeyboardEvdev {
 public:
  KeyboardEvdev(EventModifiersEvdev* modifiers,
                const EventDispatchCallback& callback);
  ~KeyboardEvdev();

  // Handlers for raw key presses & releases.
  void OnKeyChange(unsigned int code, bool down);

 private:
  void UpdateModifier(unsigned int key, bool down);
  void DispatchKey(unsigned int key, bool down);

  // Aggregated key state. There is only one bit of state per key; we do not
  // attempt to count presses of the same key on multiple keyboards.
  //
  // A key is down iff the most recent event pertaining to that key was a key
  // down event rather than a key up event. Therefore, a particular key position
  // can be considered released even if it is being depresssed on one or more
  // keyboards.
  std::bitset<KEY_CNT> key_state_;

  // Callback for dispatching events.
  EventDispatchCallback callback_;

  // Shared modifier state.
  EventModifiersEvdev* modifiers_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
