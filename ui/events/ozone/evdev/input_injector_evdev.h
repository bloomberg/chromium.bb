// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

class Event;
class CursorDelegateEvdev;
class KeyboardEvdev;
class EventModifiersEvdev;

class EVENTS_OZONE_EVDEV_EXPORT InputInjectorEvdev
    : public SystemInputInjector {
 public:
  InputInjectorEvdev(EventModifiersEvdev* modifiers,
                     CursorDelegateEvdev* cursor,
                     KeyboardEvdev* keyboard,
                     const EventDispatchCallback& callback);

  ~InputInjectorEvdev() override;

  // SystemInputInjector implementation.
  void InjectMouseButton(EventFlags button, bool down) override;
  void InjectMouseWheel(int delta_x, int delta_y) override;
  void MoveCursorTo(const gfx::PointF& location) override;
  void InjectKeyPress(DomCode physical_key, bool down) override;

 private:
  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev* modifiers_;

  // Shared cursor state.
  CursorDelegateEvdev* cursor_;

  // Shared keyboard state.
  KeyboardEvdev* keyboard_;

  // Callback for dispatching events.
  EventDispatchCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(InputInjectorEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

