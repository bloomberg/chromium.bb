// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

#include <set>

#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

struct input_event;

namespace ui {

class DeviceEventDispatcherEvdev;

class EVENTS_OZONE_EVDEV_EXPORT EventConverterEvdevImpl
    : public EventConverterEvdev {
 public:
  EventConverterEvdevImpl(int fd,
                          base::FilePath path,
                          int id,
                          InputDeviceType type,
                          const EventDeviceInfo& info,
                          CursorDelegateEvdev* cursor,
                          DeviceEventDispatcherEvdev* dispatcher);
  ~EventConverterEvdevImpl() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  bool HasKeyboard() const override;
  bool HasTouchpad() const override;
  bool HasCapsLockLed() const override;
  void SetAllowedKeys(scoped_ptr<std::set<DomCode>> allowed_keys) override;
  void AllowAllKeys() override;

  void ProcessEvents(const struct input_event* inputs, int count);

 private:
  void ConvertKeyEvent(const input_event& input);

  void ConvertMouseMoveEvent(const input_event& input);

  void DispatchMouseButton(const input_event& input);

  // Flush events delimited by EV_SYN. This is useful for handling
  // non-axis-aligned movement properly.
  void FlushEvents(const input_event& input);

  // Input modalities for this device.
  bool has_keyboard_;
  bool has_touchpad_;

  // LEDs for this device.
  bool has_caps_lock_led_;

  // Save x-axis events of relative devices to be flushed at EV_SYN time.
  int x_offset_;

  // Save y-axis events of relative devices to be flushed at EV_SYN time.
  int y_offset_;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  // The keys which should be processed. nullptr if all keys should be
  // processed.
  scoped_ptr<std::set<DomCode>> allowed_keys_;

  // Shared cursor state.
  CursorDelegateEvdev* cursor_;

  // Callbacks for dispatching events.
  DeviceEventDispatcherEvdev* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdevImpl);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

