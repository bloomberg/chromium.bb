// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_TEST_UTILS_H_
#define SERVICES_WS_EVENT_TEST_UTILS_H_

#include <string>

namespace ui {
class Event;
}

namespace ws {

// Returns a string description of event->type(), or "<null>" if |event| is
// null.
std::string EventToEventType(const ui::Event* event);

// If |event| is a LocatedEvent, returns the type as a string (using
// EventToEventType()) and the location.
std::string LocatedEventToEventTypeAndLocation(const ui::Event* event);

}  // namespace ws

#endif  // SERVICES_WS_EVENT_TEST_UTILS_H_
