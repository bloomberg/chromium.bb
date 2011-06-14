/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file provides utilty functions for creating custom pepper events
// which do not interfere with regualar pepper events.

#include "ppapi/c/pp_input_event.h"

#include "native_client/src/trusted/sel_universal/primitives.h"

namespace {
const int PP_INPUTEVENT_USER = 88;
const int PP_INPUTEVENT_TERMINATION = 90;

// ppapi does not have a user defined event notion.
// c.f. ppapi/c/pp_input_event.h
typedef struct {
  int code;
  int data1;
  int data2;
}  PP_InputEvent_User;


PP_InputEvent_User* GetUserEvent(PP_InputEvent* event) {
  return reinterpret_cast<PP_InputEvent_User*>(&event->u);
}

}  // namespace


bool IsInvalidEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_TYPE_UNDEFINED;
}


void MakeInvalidEvent(PP_InputEvent* event) {
  event->type = PP_INPUTEVENT_TYPE_UNDEFINED;
}


bool IsTerminationEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_TERMINATION;
}


void MakeTerminationEvent(PP_InputEvent* event) {
  event->type = (PP_InputEvent_Type) PP_INPUTEVENT_TERMINATION;
}


bool IsUserEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_USER;
}


int GetCodeFromUserEvent(PP_InputEvent* event) {
  return GetUserEvent(event)->code;
}


int GetData1FromUserEvent(PP_InputEvent* event) {
  return  GetUserEvent(event)->data1;
}


int GetData2FromUserEvent(PP_InputEvent* event) {
  return  GetUserEvent(event)->data2;
}


void MakeUserEvent(PP_InputEvent* event, int code, int data1, int data2) {
  event->type = (PP_InputEvent_Type) PP_INPUTEVENT_USER;
  PP_InputEvent_User* user = GetUserEvent(event);
  user->code = code;
  user->data1 = data1;
  user->data2 = data2;
}
