/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file provides utilty functions for creating custom pepper events
// which do not interfere with regualar pepper events.

#include <string>

#include "native_client/src/third_party/ppapi/c/pp_input_event.h"
#include "native_client/src/trusted/sel_universal/primitives.h"

PP_InputEvent_User* GetUserEvent(PP_InputEvent* event) {
  return reinterpret_cast<PP_InputEvent_User*>(&event->u);
}

bool IsInvalidEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_TYPE_UNDEFINED;
}


void MakeInvalidEvent(PP_InputEvent* event) {
  event->type = PP_INPUTEVENT_TYPE_UNDEFINED;
}


bool IsTerminationEvent(PP_InputEvent* event) {
  return event->type == (PP_InputEvent_Type) CUSTOM_EVENT_TERMINATION;
}


void MakeTerminationEvent(PP_InputEvent* event) {
  event->type = (PP_InputEvent_Type) CUSTOM_EVENT_TERMINATION;
}


bool IsUserEvent(PP_InputEvent* event) {
  return (int) event->type > CUSTOM_EVENT_START;
}


int GetUserEventType(PP_InputEvent* event) {
  return (int) event->type;
}


void MakeUserEvent(PP_InputEvent* event,
                   int code,
                   int callback,
                   int result,
                   void* pointer,
                   int size) {
  event->type = (PP_InputEvent_Type) code;
  PP_InputEvent_User* user = GetUserEvent(event);
  user->callback = callback;
  user->result = result;
  user->pointer = pointer;
  user->size = size;
}
