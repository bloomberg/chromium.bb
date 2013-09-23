// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_
#define UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"

namespace ui {
class Event;

// In ozone, Chrome reads events from file descriptors created from Linux device
// drivers. The |MessagePumpLibevent::Watcher| parent class provides the
// functionality to watch a file descriptor for the arrival of new data and
// notify its subclasses. Device-specific event converters turn bytes read from
// the file descriptor into |ui::Event| instances. This class provides the
// functionality needed in common across all converters: dispatching the
// |ui::Event| to aura.
class EventConverterOzone : public base::MessagePumpLibevent::Watcher {
 public:
  EventConverterOzone();
  virtual ~EventConverterOzone();

 protected:
  // Subclasses should use this method to post a task that will dispatch
  // |event| from the UI message loop. This method takes ownership of
  // |event|. |event| will be deleted at the end of the posted task.
  void DispatchEvent(scoped_ptr<ui::Event> event);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventConverterOzone);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_
