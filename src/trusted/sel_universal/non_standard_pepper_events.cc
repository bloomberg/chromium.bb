/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file provides utilty functions for creating custom pepper events
// which do not interfere with regualar pepper events.

#include <string>

#include "native_client/src/trusted/sel_universal/primitives.h"


bool IsInvalidEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_INVALID;
}


bool IsInputEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_INPUT;
}


UserEvent* MakeInvalidEvent() {
  UserEvent* event = new UserEvent;
  event->type = EVENT_TYPE_INVALID;
  return event;
}


bool IsTerminationEvent(const UserEvent* event) {
  return event->type == EVENT_TYPE_TERMINATION;
}


UserEvent* MakeTerminationEvent() {
  UserEvent* event = new UserEvent;
  event->type = EVENT_TYPE_TERMINATION;
  return event;
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
