// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "ui/events/events_export.h"
#include "ui/events/ozone/evdev/event_converter.h"
#include "ui/events/ozone/evdev/event_modifiers.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

typedef base::Callback<void(const base::FilePath& file_path)>
    EvdevDeviceCallback;

// Interface for scanning & monitoring input devices.
class DeviceManagerEvdev {
 public:
  virtual ~DeviceManagerEvdev();

  // Enumerate devices & start watching for changes.
  virtual void ScanAndStartMonitoring(
      const EvdevDeviceCallback& device_added,
      const EvdevDeviceCallback& device_removed) = 0;
};

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_EXPORT EventFactoryEvdev : public EventFactoryOzone {
 public:
  EventFactoryEvdev();
  virtual ~EventFactoryEvdev();

  virtual void StartProcessingEvents() OVERRIDE;

 private:
  // Open device at path & starting processing events.
  void AttachInputDevice(const base::FilePath& file_path);

  // Close device at path.
  void DetachInputDevice(const base::FilePath& file_path);

  // Owned per-device event converters (by path).
  std::map<base::FilePath, EventConverterEvdev*> converters_;

  // Interface for scanning & monitoring input devices.
  scoped_ptr<DeviceManagerEvdev> device_manager_;

  EventModifiersEvdev modifiers_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
