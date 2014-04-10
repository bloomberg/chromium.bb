// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

#include <bitset>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

class TouchEvent;

class EVENTS_EXPORT TouchEventConverterEvdev
    : public EventConverterEvdev,
      public base::MessagePumpLibevent::Watcher {
 public:
  enum {
    MAX_FINGERS = 11
  };
  TouchEventConverterEvdev(int fd,
                           base::FilePath path,
                           const EventDeviceInfo& info);
  virtual ~TouchEventConverterEvdev();

  // Start & stop watching for events.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  friend class MockTouchEventConverterEvdev;

  // Unsafe part of initialization.
  void Init();

  // Overidden from base::MessagePumpLibevent::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // Pressure values.
  int pressure_min_;
  int pressure_max_;  // Used to normalize pressure values.

  // Touch scaling.
  float x_scale_;
  float y_scale_;

  // Range for x-axis.
  int x_min_;
  int x_max_;

  // Range for y-axis.
  int y_min_;
  int y_max_;

  // Touch point currently being updated from the /dev/input/event* stream.
  int current_slot_;

  // File descriptor for the /dev/input/event* instance.
  int fd_;

  // Path to input device.
  base::FilePath path_;

  // Bit field tracking which in-progress touch points have been modified
  // without a syn event.
  std::bitset<MAX_FINGERS> altered_slots_;

  struct InProgressEvents {
    int x_;
    int y_;
    int id_;  // Device reported "unique" touch point id; -1 means not active
    int finger_;  // "Finger" id starting from 0; -1 means not active

    EventType type_;
    int major_;
    float pressure_;
  };

  // In-progress touch points.
  InProgressEvents events_[MAX_FINGERS];

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

