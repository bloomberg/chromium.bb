// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class EVENTS_OZONE_EVDEV_EXPORT EventConverterEvdev
    : public base::MessagePumpLibevent::Watcher {
 public:
  EventConverterEvdev(int fd, const base::FilePath& path, int id);
  ~EventConverterEvdev() override;

  int id() const { return id_; }

  const base::FilePath& path() const { return path_; }

  // Start reading events.
  void Start();

  // Stop reading events.
  void Stop();

  // Returns true of the converter is used for a touchscreen device.
  virtual bool HasTouchscreen() const;

  // Returns the size of the touchscreen device if the converter is used for a
  // touchscreen device.
  virtual gfx::Size GetTouchscreenSize() const;

  // Returns true if the converter is used with an internal device.
  virtual bool IsInternal() const;

 protected:
  // base::MessagePumpLibevent::Watcher:
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // File descriptor to read.
  int fd_;

  // Path to input device.
  base::FilePath path_;

  // Uniquely identifies an event converter.
  int id_;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
