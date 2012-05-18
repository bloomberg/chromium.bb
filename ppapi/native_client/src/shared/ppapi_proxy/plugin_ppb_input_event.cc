// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_input_event.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/untrusted/srpcgen/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"

namespace {

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::GetMainSrpcChannel;
using ppapi_proxy::kInvalidResourceId;
using ppapi_proxy::PluginInputEvent;
using ppapi_proxy::PluginResource;
using ppapi_proxy::Serialize;

// InputEvent ------------------------------------------------------------------

int32_t RequestInputEvents(PP_Instance instance, uint32_t event_classes) {
  DebugPrintf("PPB_InputEvent::RequestInputEvents: instance=%"NACL_PRId32", "
              "event_classes=%"NACL_PRIu32"\n",
              instance, event_classes);
  uint32_t success = 0;
  NaClSrpcError srpc_result =
      PpbInputEventRpcClient::PPB_InputEvent_RequestInputEvents(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(event_classes),
          false,  // false -> Don't filter.
          reinterpret_cast<int32_t*>(&success));
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    return success;
  }
  return PP_ERROR_FAILED;
}

int32_t RequestFilteringInputEvents(PP_Instance instance,
                                    uint32_t event_classes) {
  DebugPrintf("PPB_InputEvent::RequestFilteringInputEvents: instance="
              "%"NACL_PRId32", event_classes=%"NACL_PRIu32"\n",
              instance, event_classes);
  uint32_t success = 0;
  NaClSrpcError srpc_result =
      PpbInputEventRpcClient::PPB_InputEvent_RequestInputEvents(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(event_classes),
          true,  // true -> Filter.
          reinterpret_cast<int32_t*>(&success));
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    return success;
  }
  return PP_ERROR_FAILED;
}

void ClearInputEventRequest(PP_Instance instance,
                            uint32_t event_classes) {
  DebugPrintf("PPB_InputEvent::ClearInputEventRequest: instance=%"NACL_PRId32
              ", event_classes=%"NACL_PRIu32"\n",
              instance, event_classes);
    PpbInputEventRpcClient::PPB_InputEvent_ClearInputEventRequest(
        GetMainSrpcChannel(),
        instance,
        static_cast<int32_t>(event_classes));
}

// Helper RAII class to get the PluginInputEvent from the PP_Resource and hold
// it with a scoped_refptr. Also does a DebugPrintf.
class InputEventGetter {
 public:
  InputEventGetter(PP_Resource resource, const char* function_name) {
    DebugPrintf("PPB_InputEvent::%s: resource=%"NACL_PRId32"\n",
                function_name,
                resource);
    input_event_ =
        PluginResource::GetAs<PluginInputEvent>(resource);
  }

  PluginInputEvent* get() {
    return input_event_.get();
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(InputEventGetter);
  scoped_refptr<PluginInputEvent> input_event_;
};
// This macro is for starting a resource function. It makes sure resource_arg
// is of type PluginInputEvent, and returns error_return if it's not.
#define BEGIN_RESOURCE_THUNK(function_name, resource_arg, error_return) \
  InputEventGetter input_event(resource_arg, function_name); \
  if (!input_event.get()) { \
    return error_return; \
  } \
  do {} while(0)

PP_Bool IsInputEvent(PP_Resource resource) {
  BEGIN_RESOURCE_THUNK("IsInputEvent", resource, PP_FALSE);
  return PP_TRUE;
}

#define IMPLEMENT_RESOURCE_THUNK(function, resource_arg, error_return) \
  BEGIN_RESOURCE_THUNK(#function, resource_arg, error_return); \
  return input_event.get()->function()

#define IMPLEMENT_RESOURCE_THUNK1(function, resource_arg, arg, error_return) \
  BEGIN_RESOURCE_THUNK(#function, resource_arg, error_return); \
  return input_event.get()->function(arg)

PP_InputEvent_Type GetType(PP_Resource event) {
  IMPLEMENT_RESOURCE_THUNK(GetType, event, PP_INPUTEVENT_TYPE_UNDEFINED);
}

PP_TimeTicks GetTimeStamp(PP_Resource event) {
  IMPLEMENT_RESOURCE_THUNK(GetTimeStamp, event, 0.0);
}

uint32_t GetModifiers(PP_Resource event) {
  IMPLEMENT_RESOURCE_THUNK(GetModifiers, event, 0);
}

// Mouse -----------------------------------------------------------------------

PP_Resource CreateMouseInputEvent1_1(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     PP_InputEvent_MouseButton mouse_button,
                                     const PP_Point* mouse_position,
                                     int32_t click_count,
                                     const PP_Point* mouse_movement) {
  DebugPrintf("PPB_InputEvent::CreateMouseInputEvent: instance="
              "%"NACL_PRId32", type=%d, time_stamp=%lf, modifiers="
              "%"NACL_PRIu32", mouse_button=%d, x=%"NACL_PRId32", y="
              "%"NACL_PRId32", click_count=%d, movement_x="
              "%"NACL_PRId32", movement_y=%"NACL_PRId32"\n",
              instance, type, time_stamp, modifiers, mouse_button,
              mouse_position->x, mouse_position->y, click_count,
              mouse_movement->x, mouse_movement->y);
  PP_Resource resource_id = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbInputEventRpcClient::PPB_InputEvent_CreateMouseInputEvent(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(type),
          time_stamp,
          static_cast<int32_t>(modifiers),
          static_cast<int32_t>(mouse_button),
          mouse_position->x,
          mouse_position->y,
          click_count,
          mouse_movement->x,
          mouse_movement->y,
          &resource_id);
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource_id;
  return kInvalidResourceId;
}

PP_Resource CreateMouseInputEvent1_0(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     PP_InputEvent_MouseButton mouse_button,
                                     const PP_Point* mouse_position,
                                     int32_t click_count) {
  PP_Point mouse_movement = PP_MakePoint(0, 0);
  return CreateMouseInputEvent1_1(instance, type, time_stamp, modifiers,
                                  mouse_button, mouse_position, click_count,
                                  &mouse_movement);

}

PP_Bool IsMouseInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
                     type == PP_INPUTEVENT_TYPE_MOUSEUP ||
                     type == PP_INPUTEVENT_TYPE_MOUSEMOVE ||
                     type == PP_INPUTEVENT_TYPE_MOUSEENTER ||
                     type == PP_INPUTEVENT_TYPE_MOUSELEAVE ||
                     type == PP_INPUTEVENT_TYPE_CONTEXTMENU);
}

PP_InputEvent_MouseButton GetMouseButton(PP_Resource mouse_event) {
  IMPLEMENT_RESOURCE_THUNK(GetMouseButton, mouse_event,
                           PP_INPUTEVENT_MOUSEBUTTON_NONE);
}

PP_Point GetMousePosition(PP_Resource mouse_event) {
  IMPLEMENT_RESOURCE_THUNK(GetMousePosition, mouse_event, PP_MakePoint(0, 0));
}

int32_t GetMouseClickCount(PP_Resource mouse_event) {
  IMPLEMENT_RESOURCE_THUNK(GetMouseClickCount, mouse_event, 0);
}

PP_Point GetMouseMovement(PP_Resource mouse_event) {
  IMPLEMENT_RESOURCE_THUNK(GetMouseMovement, mouse_event, PP_MakePoint(0, 0));
}

// Wheel -----------------------------------------------------------------------

PP_Resource CreateWheelInputEvent(PP_Instance instance,
                                  PP_TimeTicks time_stamp,
                                  uint32_t modifiers,
                                  const PP_FloatPoint* wheel_delta,
                                  const PP_FloatPoint* wheel_ticks,
                                  PP_Bool scroll_by_page) {
  DebugPrintf("PPB_InputEvent::CreateWheelInputEvent: instance="
              "%"NACL_PRId32", time_stamp=%lf, modifiers="
              "%"NACL_PRIu32", delta.x=%d, delta.y=%d, ticks.x=%d, ticks.y=%d, "
              "scroll_by_page=%s\n",
              instance, time_stamp, modifiers, wheel_delta->x, wheel_delta->y,
              wheel_ticks->x, wheel_ticks->y,
              (scroll_by_page ? "true" : "false"));
  PP_Resource resource_id = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbInputEventRpcClient::PPB_InputEvent_CreateWheelInputEvent(
          GetMainSrpcChannel(),
          instance,
          static_cast<double>(time_stamp),
          static_cast<int32_t>(modifiers),
          wheel_delta->x,
          wheel_delta->y,
          wheel_ticks->x,
          wheel_ticks->y,
          static_cast<int32_t>(scroll_by_page),
          &resource_id);
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource_id;
  return kInvalidResourceId;
}

PP_Bool IsWheelInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_WHEEL);
}

PP_FloatPoint GetWheelDelta(PP_Resource wheel_event) {
  IMPLEMENT_RESOURCE_THUNK(GetWheelDelta, wheel_event, PP_MakeFloatPoint(0, 0));
}

PP_FloatPoint GetWheelTicks(PP_Resource wheel_event) {
  IMPLEMENT_RESOURCE_THUNK(GetWheelTicks, wheel_event, PP_MakeFloatPoint(0, 0));
}

PP_Bool GetWheelScrollByPage(PP_Resource wheel_event) {
  IMPLEMENT_RESOURCE_THUNK(GetWheelScrollByPage, wheel_event, PP_FALSE);
}

// Keyboard --------------------------------------------------------------------

PP_Resource CreateKeyboardInputEvent(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     uint32_t key_code,
                                     struct PP_Var character_text) {
  DebugPrintf("PPB_InputEvent::CreateKeyboardInputEvent: instance="
              "%"NACL_PRId32", type=%d, time_stamp=%lf, modifiers="
              "%"NACL_PRIu32", key_code=%"NACL_PRIu32"\n",
              instance, type, time_stamp, modifiers, key_code);
  // Serialize the character_text Var.
  uint32_t text_size = 0;
  nacl::scoped_array<char> text_bytes(Serialize(&character_text, 1,
                                                &text_size));
  PP_Resource resource_id = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbInputEventRpcClient::PPB_InputEvent_CreateKeyboardInputEvent(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(type),
          static_cast<double>(time_stamp),
          static_cast<int32_t>(modifiers),
          static_cast<int32_t>(key_code),
          text_size,
          text_bytes.get(),
          &resource_id);
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource_id;
  return kInvalidResourceId;
}

PP_Bool IsKeyboardInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_KEYDOWN ||
                     type == PP_INPUTEVENT_TYPE_KEYUP ||
                     type == PP_INPUTEVENT_TYPE_CHAR);
}

uint32_t GetKeyCode(PP_Resource key_event) {
  IMPLEMENT_RESOURCE_THUNK(GetKeyCode, key_event, 0);
}

PP_Var GetCharacterText(PP_Resource character_event) {
  IMPLEMENT_RESOURCE_THUNK(GetCharacterText, character_event,
                           PP_MakeUndefined());
}

// Keyboard_Dev ----------------------------------------------------------------

PP_Bool SetUsbKeyCode(PP_Resource key_event, uint32_t usb_key_code) {
  IMPLEMENT_RESOURCE_THUNK1(SetUsbKeyCode, key_event, usb_key_code,
                            PP_FALSE);
}

uint32_t GetUsbKeyCode(PP_Resource key_event) {
  IMPLEMENT_RESOURCE_THUNK(GetUsbKeyCode, key_event, 0);
}

}  // namespace

namespace ppapi_proxy {

// static
const PPB_InputEvent* PluginInputEvent::GetInterface() {
  static const PPB_InputEvent input_event_interface = {
    RequestInputEvents,
    RequestFilteringInputEvents,
    ClearInputEventRequest,
    IsInputEvent,
    ::GetType,
    ::GetTimeStamp,
    ::GetModifiers
  };
  return &input_event_interface;
}

// static
const PPB_MouseInputEvent_1_0* PluginInputEvent::GetMouseInterface1_0() {
  static const PPB_MouseInputEvent_1_0 mouse_input_event_interface = {
    CreateMouseInputEvent1_0,
    IsMouseInputEvent,
    ::GetMouseButton,
    ::GetMousePosition,
    ::GetMouseClickCount
  };
  return &mouse_input_event_interface;
}

// static
const PPB_MouseInputEvent* PluginInputEvent::GetMouseInterface1_1() {
  static const PPB_MouseInputEvent mouse_input_event_interface = {
    CreateMouseInputEvent1_1,
    IsMouseInputEvent,
    ::GetMouseButton,
    ::GetMousePosition,
    ::GetMouseClickCount,
    ::GetMouseMovement
  };
  return &mouse_input_event_interface;
}

// static
const PPB_WheelInputEvent* PluginInputEvent::GetWheelInterface() {
  static const PPB_WheelInputEvent wheel_input_event_interface = {
    CreateWheelInputEvent,
    IsWheelInputEvent,
    ::GetWheelDelta,
    ::GetWheelTicks,
    ::GetWheelScrollByPage
  };
  return &wheel_input_event_interface;
}

// static
const PPB_KeyboardInputEvent* PluginInputEvent::GetKeyboardInterface() {
  static const PPB_KeyboardInputEvent keyboard_input_event_interface = {
    CreateKeyboardInputEvent,
    IsKeyboardInputEvent,
    ::GetKeyCode,
    ::GetCharacterText
  };
  return &keyboard_input_event_interface;
}

// static
const PPB_KeyboardInputEvent_Dev* PluginInputEvent::GetKeyboardInterface_Dev() {
  static const PPB_KeyboardInputEvent_Dev keyboard_input_event_dev_interface = {
    ::SetUsbKeyCode,
    ::GetUsbKeyCode,
  };
  return &keyboard_input_event_dev_interface;
}

PluginInputEvent::PluginInputEvent()
    : character_text_(PP_MakeUndefined()) {
}

void PluginInputEvent::Init(const InputEventData& input_event_data,
                            PP_Var character_text) {
  input_event_data_ = input_event_data;
  character_text_ = character_text;
}

PP_InputEvent_Type PluginInputEvent::GetType() const {
  return input_event_data_.event_type;
}

PP_TimeTicks PluginInputEvent::GetTimeStamp() const {
  return input_event_data_.event_time_stamp;
}

uint32_t PluginInputEvent::GetModifiers() const {
  return input_event_data_.event_modifiers;
}

PP_InputEvent_MouseButton PluginInputEvent::GetMouseButton() const {
  return input_event_data_.mouse_button;
}

PP_Point PluginInputEvent::GetMousePosition() const {
  return input_event_data_.mouse_position;
}

int32_t PluginInputEvent::GetMouseClickCount() const {
  return input_event_data_.mouse_click_count;
}

PP_Point PluginInputEvent::GetMouseMovement() const {
  return input_event_data_.mouse_movement;
}

PP_FloatPoint PluginInputEvent::GetWheelDelta() const {
  return input_event_data_.wheel_delta;
}

PP_FloatPoint PluginInputEvent::GetWheelTicks() const {
  return input_event_data_.wheel_ticks;
}

PP_Bool PluginInputEvent::GetWheelScrollByPage() const {
  return input_event_data_.wheel_scroll_by_page;
}

uint32_t PluginInputEvent::GetKeyCode() const {
  return input_event_data_.key_code;
}

PP_Var PluginInputEvent::GetCharacterText() const {
  PPBVarInterface()->AddRef(character_text_);
  return character_text_;
}

PP_Bool PluginInputEvent::SetUsbKeyCode(uint32_t usb_key_code) {
  input_event_data_.usb_key_code = usb_key_code;
  return PP_TRUE;
}

uint32_t PluginInputEvent::GetUsbKeyCode() const {
  return input_event_data_.usb_key_code;
}

PluginInputEvent::~PluginInputEvent() {
  // Release the character text.  This is a no-op if it's not a string.
  PPBVarInterface()->Release(character_text_);
}

}  // namespace ppapi_proxy
