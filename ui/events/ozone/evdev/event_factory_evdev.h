// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "ui/events/events_export.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

class DeviceManagerEvdev;

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_EXPORT EventFactoryEvdev : public EventFactoryOzone {
 public:
  EventFactoryEvdev();
  virtual ~EventFactoryEvdev();

  virtual void StartProcessingEvents() OVERRIDE;

  // Set task runner to use for device polling & initialization.
  virtual void SetFileTaskRunner(scoped_refptr<base::TaskRunner> task_runner)
      OVERRIDE;

 private:
  // Open device at path & starting processing events (on UI thread).
  void AttachInputDevice(const base::FilePath& file_path,
                         scoped_ptr<EventConverterEvdev> converter);

  // Close device at path (on UI thread).
  void DetachInputDevice(const base::FilePath& file_path);

  // Callback for device add (on UI thread).
  void OnDeviceAdded(const base::FilePath& path);

  // Callback for device remove (on UI thread).
  void OnDeviceRemoved(const base::FilePath& path);

  // Owned per-device event converters (by path).
  std::map<base::FilePath, EventConverterEvdev*> converters_;

  // Interface for scanning & monitoring input devices.
  scoped_ptr<DeviceManagerEvdev> device_manager_;

  // Task runner for event dispatch.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // Task runner for file I/O.
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev modifiers_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<EventFactoryEvdev> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
