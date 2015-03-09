// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_

#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/gfx/geometry/size.h"

struct input_event;

namespace ui {
enum class DomCode;

class EVENTS_OZONE_EVDEV_EXPORT EventConverterEvdev
    : public base::MessagePumpLibevent::Watcher {
 public:
  EventConverterEvdev(int fd,
                      const base::FilePath& path,
                      int id,
                      InputDeviceType type);
  ~EventConverterEvdev() override;

  int id() const { return id_; }

  const base::FilePath& path() const { return path_; }

  InputDeviceType type() const { return type_; }

  void set_ignore_events(bool ignore_events) { ignore_events_ = ignore_events; }

  // Start reading events.
  void Start();

  // Stop reading events.
  void Stop();

  // Cleanup after we stop reading events (release buttons, etc).
  virtual void OnStopped();

  // Returns true if the converter is used for a keyboard device.
  virtual bool HasKeyboard() const;

  // Returns true if the converter is used for a mouse device;
  virtual bool HasMouse() const;

  // Returns true if the converter is used for a touchpad device.
  virtual bool HasTouchpad() const;

  // Returns true if the converter is used for a touchscreen device.
  virtual bool HasTouchscreen() const;

  // Returns true if the converter is used for a device with a caps lock LED.
  virtual bool HasCapsLockLed() const;

  // Returns the size of the touchscreen device if the converter is used for a
  // touchscreen device.
  virtual gfx::Size GetTouchscreenSize() const;

  // Returns the number of touch points this device supports. Should not be
  // called unless HasTouchscreen() returns true
  virtual int GetTouchPoints() const;

  // Sets which keyboard keys should be processed.
  virtual void SetAllowedKeys(scoped_ptr<std::set<DomCode>> allowed_keys);

  // Allows all keys to be processed.
  virtual void AllowAllKeys();

  // Update caps lock LED state.
  virtual void SetCapsLockLed(bool enabled);

  // Helper to generate a base::TimeDelta from an input_event's time
  static base::TimeDelta TimeDeltaFromInputEvent(const input_event& event);

 protected:
  // base::MessagePumpLibevent::Watcher:
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // File descriptor to read.
  int fd_;

  // Path to input device.
  base::FilePath path_;

  // Uniquely identifies an event converter.
  int id_;

  // Type (internal or external).
  InputDeviceType type_;

  // Whether events from the device should be ignored.
  bool ignore_events_;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
