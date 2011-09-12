// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_InputEvent functions.

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_input_event.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/trusted/srpcgen/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"


using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeserializeTo;

void PpbInputEventRpcServer::PPB_InputEvent_RequestInputEvents(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t event_classes,
    int32_t filtering,
    int32_t* success)  {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = PP_ERROR_FAILED;
  DebugPrintf("PPB_InputEvent::RequestInputEvents: instance=%"NACL_PRIu32", "
              "event_classes=%"NACL_PRIu32", filtering=%"NACL_PRId32"\n",
              instance, static_cast<uint32_t>(event_classes), filtering);
  const PPB_InputEvent* input_event_if = ppapi_proxy::PPBInputEventInterface();
  if (!input_event_if) {
    *success = PP_ERROR_NOTSUPPORTED;
    DebugPrintf("PPB_InputEvent::RequestInputEvents: success=%"NACL_PRId32"\n",
                *success);
    return;
  }
  if (filtering) {
    *success = input_event_if->RequestFilteringInputEvents(
        instance,
        static_cast<uint32_t>(event_classes));
  } else {
    *success = input_event_if->RequestInputEvents(
        instance,
        static_cast<uint32_t>(event_classes));
  }
  DebugPrintf("PPB_InputEvent::RequestInputEvents: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInputEventRpcServer::PPB_InputEvent_ClearInputEventRequest(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t event_classes)  {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPB_InputEvent::ClearInputEventRequest: instance=%"NACL_PRIu32
              ", event_classes=%"NACL_PRIu32"\n",
              instance, static_cast<uint32_t>(event_classes));
  const PPB_InputEvent* input_event_if = ppapi_proxy::PPBInputEventInterface();
  if (!input_event_if) {
    return;
  }
  input_event_if->ClearInputEventRequest(instance,
                                         static_cast<uint32_t>(event_classes));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInputEventRpcServer::PPB_InputEvent_CreateMouseInputEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t type,  // PP_InputEvent_Type
    double time_stamp,  // PP_TimeTicks
    int32_t modifiers,  // uint32_t
    int32_t mouse_button,  // PP_InputEvent_MouseButton
    int32_t position_x,  // PP_Point.x
    int32_t position_y,  // PP_Point.y
    int32_t click_count,
    int32_t movement_x,  // PP_Point.x
    int32_t movement_y,  // PP_Point.y
    PP_Resource* resource_id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *resource_id = 0;
  // TODO(dmichael): Add DebugPrintf.
  const PPB_MouseInputEvent* input_event_if =
      ppapi_proxy::PPBMouseInputEventInterface();
  if (!input_event_if) {
    return;
  }
  PP_Point mouse_position = { position_x, position_y };
  PP_Point mouse_movement = { movement_x, movement_y };
  *resource_id = input_event_if->Create(
        instance,
        static_cast<PP_InputEvent_Type>(type),
        static_cast<PP_TimeTicks>(time_stamp),
        static_cast<uint32_t>(modifiers),
        static_cast<PP_InputEvent_MouseButton>(mouse_button),
        &mouse_position,
        click_count,
        &mouse_movement);
  DebugPrintf("PPB_InputEvent::CreateMouseInputEvent: resource_id="
              "%"NACL_PRId32"\n",
              *resource_id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInputEventRpcServer::PPB_InputEvent_CreateKeyboardInputEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t type,  // PP_InputEvent_Type
    double time_stamp,  // PP_TimeTicks
    int32_t modifiers,  // uint32_t
    int32_t key_code,
    nacl_abi_size_t character_size,
    char* character_bytes,
    PP_Resource* resource_id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *resource_id = 0;
  // TODO(dmichael): Add DebugPrintf.
  const PPB_KeyboardInputEvent* input_event_if =
      ppapi_proxy::PPBKeyboardInputEventInterface();
  if (!input_event_if) {
    return;
  }
  PP_Var character_text;
  if (!DeserializeTo(rpc->channel, character_bytes, character_size, 1,
                     &character_text)) {
    return;
  }

  *resource_id = input_event_if->Create(instance,
                                        static_cast<PP_InputEvent_Type>(type),
                                        static_cast<PP_TimeTicks>(time_stamp),
                                        static_cast<uint32_t>(modifiers),
                                        key_code,
                                        character_text);
  DebugPrintf("PPB_InputEvent::CreateKeyboardInputEvent: resource_id="
              "%"NACL_PRId32"\n",
              *resource_id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInputEventRpcServer::PPB_InputEvent_CreateWheelInputEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    double time_stamp,  // PP_TimeTicks
    int32_t modifiers,  // uint32_t
    double delta_x,  // PP_FloatPoint.x
    double delta_y,  // PP_FloatPoint.y
    double ticks_x,  // PP_FloatPoint.x
    double ticks_y,  // PP_FloatPoint.y
    int32_t scroll_by_page,  // PP_Bool
    PP_Resource* resource_id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *resource_id = 0;
  // TODO(dmichael): Add DebugPrintf.
  const PPB_WheelInputEvent* input_event_if =
      ppapi_proxy::PPBWheelInputEventInterface();
  if (!input_event_if) {
    return;
  }
  PP_FloatPoint wheel_delta = { static_cast<float>(delta_x),
                                static_cast<float>(delta_y) };
  PP_FloatPoint wheel_ticks = { static_cast<float>(ticks_x),
                                static_cast<float>(ticks_y) };
  *resource_id = input_event_if->Create(
        instance,
        static_cast<PP_TimeTicks>(time_stamp),
        static_cast<uint32_t>(modifiers),
        &wheel_delta,
        &wheel_ticks,
        static_cast<PP_Bool>(scroll_by_page));
  DebugPrintf("PPB_InputEvent::CreateWheelInputEvent: resource_id="
              "%"NACL_PRId32"\n",
              *resource_id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

