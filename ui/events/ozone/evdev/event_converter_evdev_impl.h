// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"

struct input_event;

namespace ui {

class EVENTS_OZONE_EVDEV_EXPORT EventConverterEvdevImpl
    : public EventConverterEvdev {
 public:
  EventConverterEvdevImpl(int fd,
                          base::FilePath path,
                          int id,
                          EventModifiersEvdev* modifiers,
                          CursorDelegateEvdev* cursor,
                          KeyboardEvdev* keyboard,
                          const EventDispatchCallback& callback);
  ~EventConverterEvdevImpl() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;

  void ProcessEvents(const struct input_event* inputs, int count);

 private:
  void ConvertKeyEvent(const input_event& input);

  void ConvertMouseMoveEvent(const input_event& input);

  void DispatchMouseButton(const input_event& input);

  // Flush events delimited by EV_SYN. This is useful for handling
  // non-axis-aligned movement properly.
  void FlushEvents();

  // Save x-axis events of relative devices to be flushed at EV_SYN time.
  int x_offset_;

  // Save y-axis events of relative devices to be flushed at EV_SYN time.
  int y_offset_;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  // Shared cursor state.
  CursorDelegateEvdev* cursor_;

  // Shared keyboard state.
  KeyboardEvdev* keyboard_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev* modifiers_;

  // Callback for dispatching events.
  EventDispatchCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdevImpl);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

