// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) InputInjectorEvdev : public SystemInputInjector {
 public:
  InputInjectorEvdev(std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
                     CursorDelegateEvdev* cursor);

  ~InputInjectorEvdev() override;

  // SystemInputInjector implementation.
  void InjectMouseButton(EventFlags button, bool down) override;
  void InjectMouseWheel(int delta_x, int delta_y) override;
  void MoveCursorTo(const gfx::PointF& location) override;
  void InjectKeyEvent(DomCode physical_key,
                      bool down,
                      bool suppress_auto_repeat) override;

 private:
  // Shared cursor state.
  CursorDelegateEvdev* const cursor_;

  // Interface for dispatching events.
  const std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(InputInjectorEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

