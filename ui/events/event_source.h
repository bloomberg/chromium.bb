// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_SOURCE_H_
#define UI_EVENTS_EVENT_SOURCE_H_

#include "ui/events/event_dispatcher.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;
class EventProcessor;

// EventSource receives events from the native platform (e.g. X11, win32 etc.)
// and sends the events to an EventProcessor.
class EVENTS_EXPORT EventSource {
 public:
  virtual ~EventSource() {}

  virtual EventProcessor* GetEventProcessor() = 0;

 protected:
  EventDispatchDetails SendEventToProcessor(Event* event);
};

}  // namespace ui

#endif // UI_EVENTS_EVENT_SOURCE_H_
