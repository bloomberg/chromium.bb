// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;
class EventModifiersEvdev;

// Base class for device-specific evdev event conversion.
class EVENTS_EXPORT EventConverterEvdev {
 public:
  EventConverterEvdev();
  virtual ~EventConverterEvdev();

  // Start converting events.
  virtual void Start() = 0;

  // Stop converting events.
  virtual void Stop() = 0;

 protected:
  // Subclasses should use this method to post a task that will dispatch
  // |event| from the UI message loop. This method takes ownership of
  // |event|. |event| will be deleted at the end of the posted task.
  virtual void DispatchEvent(scoped_ptr<ui::Event> event);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
