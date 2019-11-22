// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

#include <bitset>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

struct input_event;

namespace ui {

class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) EventConverterEvdevImpl
    : public EventConverterEvdev {
 public:
  EventConverterEvdevImpl(base::ScopedFD fd,
                          base::FilePath path,
                          int id,
                          const EventDeviceInfo& info,
                          CursorDelegateEvdev* cursor,
                          DeviceEventDispatcherEvdev* dispatcher);
  ~EventConverterEvdevImpl() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  bool HasKeyboard() const override;
  bool HasTouchpad() const override;
  bool HasCapsLockLed() const override;
  void SetKeyFilter(bool enable_filter,
                    std::vector<DomCode> allowed_keys) override;
  void OnDisabled() override;

  void ProcessEvents(const struct input_event* inputs, int count);

 private:
  void ConvertKeyEvent(const input_event& input);
  void ConvertMouseMoveEvent(const input_event& input);
  void OnKeyChange(unsigned int key,
                   bool down,
                   const base::TimeTicks& timestamp);
  void ReleaseKeys();
  void ReleaseMouseButtons();
  void OnLostSync();
  void DispatchMouseButton(const input_event& input);
  void OnButtonChange(int code, bool down, base::TimeTicks timestamp);

  // Flush events delimited by EV_SYN. This is useful for handling
  // non-axis-aligned movement properly.
  void FlushEvents(const input_event& input);

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Input modalities for this device.
  bool has_keyboard_;
  bool has_touchpad_;

  // LEDs for this device.
  bool has_caps_lock_led_;

  // Save x-axis events of relative devices to be flushed at EV_SYN time.
  int x_offset_ = 0;

  // Save y-axis events of relative devices to be flushed at EV_SYN time.
  int y_offset_ = 0;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FdWatchController controller_;

  // The evdev codes of the keys which should be blocked.
  std::bitset<KEY_CNT> blocked_keys_;

  // Pressed keys bitset.
  std::bitset<KEY_CNT> key_state_;

  // Last mouse button state.
  static const int kMouseButtonCount = BTN_JOYSTICK - BTN_MOUSE;
  std::bitset<kMouseButtonCount> mouse_button_state_;

  // Shared cursor state.
  CursorDelegateEvdev* const cursor_;

  // Callbacks for dispatching events.
  DeviceEventDispatcherEvdev* const dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdevImpl);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

