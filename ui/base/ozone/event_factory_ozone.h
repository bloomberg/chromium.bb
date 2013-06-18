// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_OZONE_EVENT_FACTORY_OZONE_H_
#define UI_BASE_OZONE_EVENT_FACTORY_OZONE_H_

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/base/ozone/event_converter_ozone.h"
#include "ui/base/ui_export.h"

namespace ui {

// Sets up and manages watcher instances for event sources in /dev/input/*.
class UI_EXPORT EventFactoryOzone {
 public:
  EventFactoryOzone();
  virtual ~EventFactoryOzone();

  // Opens /dev/input/* event sources as appropriate and set up watchers.
  void CreateEvdevWatchers();

 private:
  // FileDescriptorWatcher instances for each watched source of events.
  ScopedVector<EventConverterOzone> evdev_watchers_;
  ScopedVector<base::MessagePumpLibevent::FileDescriptorWatcher>
      fd_controllers_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryOzone);
};

}  // namespace ui

#endif  // UI_BASE_OZONE_EVENT_FACTORY_OZONE_H_
