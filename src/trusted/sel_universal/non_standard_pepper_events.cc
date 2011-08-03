/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file provides utilty functions for creating custom pepper events
// which do not interfere with regualar pepper events.

#include <sstream>
#include <string>
#include "native_client/src/trusted/sel_universal/primitives.h"


bool IsInvalidEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_INVALID;
}


bool IsInputEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_INPUT;
}


bool IsTerminationEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_TERMINATION;
}


UserEvent* MakeUserEvent(EVENT_TYPE type,
                         int callback,
                         int result,
                         void* pointer,
                         int size) {
  UserEvent* event = new UserEvent;
  event->type = type;
  event->callback = callback;
  event->result = result;
  event->pointer = pointer;
  event->size = size;
  return event;
}


UserEvent* MakeInvalidEvent() {
  return MakeUserEvent(EVENT_TYPE_INVALID, 0, 0, NULL, 0);
}


UserEvent* MakeTerminationEvent() {
  return MakeUserEvent(EVENT_TYPE_TERMINATION, 0, 0, NULL, 0);
}


std::string StringifyEventType(const UserEvent* event) {
  std::stringstream s;
  switch (event->type) {
    default:
      s <<  "UNKNOWN(" << event->type << ")";
      return s.str();
    case EVENT_TYPE_TERMINATION:
      return "TERMINATION";
    case EVENT_TYPE_OPEN_CALLBACK:
      return "OPEN_CALLBACK";
    case EVENT_TYPE_TIMER_CALLBACK:
      return "TIMER_CALLBACK";
    case EVENT_TYPE_READ_CALLBACK:
      return "READ_CALLBACK";
    case EVENT_TYPE_FLUSH_CALLBACK:
      return "FLUSH_CALLBACK";
    case EVENT_TYPE_INVALID:
      return "INVALID";
    case EVENT_TYPE_INIT_AUDIO:
      return "INIT_AUDIO";
    case EVENT_TYPE_INPUT:
      return "TYPE_INPUT";
  }
}


std::string StringifyEvent(const UserEvent* event) {
  std::stringstream s;
  s << "type: " << StringifyEventType(event)
    << "  cb: " << event->callback
    << "  res: " << event->result
    << "  p: " << reinterpret_cast<void*>(event->pointer)
    << "  size: " << event->size;
  return s.str();
}
