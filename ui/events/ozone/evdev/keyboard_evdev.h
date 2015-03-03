// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_

#include <bitset>
#include <linux/input.h>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {

class EventModifiersEvdev;
enum class DomCode;

// Keyboard for evdev.
//
// This object is responsible for combining all attached keyboards into
// one logical keyboard, applying modifiers & implementing key repeat.
//
// It also currently also applies the layout.
class EVENTS_OZONE_EVDEV_EXPORT KeyboardEvdev {
 public:
  KeyboardEvdev(EventModifiersEvdev* modifiers,
                KeyboardLayoutEngine* keyboard_layout_engine,
                const EventDispatchCallback& callback);
  ~KeyboardEvdev();

  // Handlers for raw key presses & releases.
  void OnKeyChange(unsigned int code, bool down, base::TimeDelta timestamp);

  // Release all pressed keys.
  void Reset();

  // Handle Caps Lock modifier.
  void SetCapsLockEnabled(bool enabled);
  bool IsCapsLockEnabled();

  // Configuration for key repeat.
  bool IsAutoRepeatEnabled();
  void SetAutoRepeatEnabled(bool enabled);
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval);
  void GetAutoRepeatRate(base::TimeDelta* delay, base::TimeDelta* interval);

 private:
  void UpdateModifier(int modifier_flag, bool down);
  void UpdateCapsLockLed();
  void UpdateKeyRepeat(unsigned int key, bool down);
  void StartKeyRepeat(unsigned int key);
  void StopKeyRepeat();
  void OnRepeatDelayTimeout();
  void OnRepeatIntervalTimeout();
  void DispatchKey(unsigned int key,
                   bool down,
                   bool repeat,
                   base::TimeDelta timestamp);

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

  // Shared layout engine.
  KeyboardLayoutEngine* keyboard_layout_engine_;

  // Key repeat state.
  bool repeat_enabled_;
  unsigned int repeat_key_;
  base::TimeDelta repeat_delay_;
  base::TimeDelta repeat_interval_;
  base::OneShotTimer<KeyboardEvdev> repeat_delay_timer_;
  base::RepeatingTimer<KeyboardEvdev> repeat_interval_timer_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
