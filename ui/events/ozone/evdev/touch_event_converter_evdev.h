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
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

class TouchEvent;

class DeviceEventDispatcherEvdev;

class EVENTS_OZONE_EVDEV_EXPORT TouchEventConverterEvdev
    : public EventConverterEvdev {
 public:
  enum { MAX_FINGERS = 20 };
  TouchEventConverterEvdev(int fd,
                           base::FilePath path,
                           int id,
                           InputDeviceType type,
                           DeviceEventDispatcherEvdev* dispatcher);
  ~TouchEventConverterEvdev() override;

  // EventConverterEvdev:
  bool HasTouchscreen() const override;
  gfx::Size GetTouchscreenSize() const override;
  int GetTouchPoints() const override;

  // Unsafe part of initialization.
  virtual void Initialize(const EventDeviceInfo& info);

 private:
  friend class MockTouchEventConverterEvdev;

  struct InProgressEvents {
    InProgressEvents();

    bool altered_;
    float x_;
    float y_;
    int id_;  // Device reported "unique" touch point id; -1 means not active
    int finger_;  // "Finger" id starting from 0; -1 means not active

    EventType type_;
    float radius_x_;
    float radius_y_;
    float pressure_;
  };

  // Overidden from base::MessagePumpLibevent::Watcher.
  void OnFileCanReadWithoutBlocking(int fd) override;

  virtual bool Reinitialize();

  void ProcessInputEvent(const input_event& input);
  void ProcessAbs(const input_event& input);
  void ProcessSyn(const input_event& input);

  void ReportEvent(int touch_id,
      const InProgressEvents& event, const base::TimeDelta& delta);
  void ReportEvents(base::TimeDelta delta);

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher_;

  // Set if we have seen a SYN_DROPPED and not yet re-synced with the device.
  bool syn_dropped_;

  // Set if this is a type A device (uses SYN_MT_REPORT).
  bool is_type_a_;

  // Pressure values.
  int pressure_min_;
  int pressure_max_;  // Used to normalize pressure values.

  // Input range for x-axis.
  float x_min_tuxels_;
  float x_num_tuxels_;

  // Input range for y-axis.
  float y_min_tuxels_;
  float y_num_tuxels_;

  // Size of the touchscreen as reported by the driver.
  gfx::Size native_size_;

  // Number of touch points reported by driver
  int touch_points_;

  // Touch point currently being updated from the /dev/input/event* stream.
  size_t current_slot_;

  // In-progress touch points.
  std::vector<InProgressEvents> events_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
