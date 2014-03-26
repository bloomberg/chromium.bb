// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_PLATFORM_EVENT_OBSERVER_H_
#define UI_EVENTS_PLATFORM_PLATFORM_EVENT_OBSERVER_H_

#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_types.h"

namespace ui {

// PlatformEventObserver can be installed on a PlatformEventSource, and it
// receives all events that are dispatched to the dispatchers.
class EVENTS_EXPORT PlatformEventObserver {
 public:
  enum EventStatus {
    EVENT_STATUS_CONTINUE,
    EVENT_STATUS_HANDLED
  };

  // This is called before the dispatcher receives the event. The observer can
  // consume the event and stop the event from reaching the dispatcher or other
  // observers by returning EVENT_STATUS_HANDLED from this function. The event
  // dispatch continues as usual if this returns EVENT_STATUS_CONTINUE.
  virtual EventStatus WillProcessEvent(const PlatformEvent& event) = 0;

  // This is called after the event has been dispatched to the dispatcher(s).
  virtual void DidProcessEvent(const PlatformEvent& event) = 0;

 protected:
  virtual ~PlatformEventObserver() {}
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_PLATFORM_EVENT_OBSERVER_H_
