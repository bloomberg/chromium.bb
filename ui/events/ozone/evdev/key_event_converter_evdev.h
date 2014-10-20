// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_

#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"

struct input_event;

namespace ui {

class EVENTS_OZONE_EVDEV_EXPORT KeyEventConverterEvdev
    : public EventConverterEvdev {
 public:
  KeyEventConverterEvdev(int fd,
                         base::FilePath path,
                         int id,
                         KeyboardEvdev* keyboard);
  virtual ~KeyEventConverterEvdev();

  // EventConverterEvdev:
  virtual void OnFileCanReadWithoutBlocking(int fd) override;

  void ProcessEvents(const struct input_event* inputs, int count);

 private:
  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  // Shared keyboard state.
  KeyboardEvdev* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventConverterEvdev);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_

