// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_input_event.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/input_event_data.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/trusted/srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp_input_event.h"

namespace ppapi_proxy {

namespace {

PP_Bool HandleInputEvent(PP_Instance instance, PP_Resource input_event) {
  DebugPrintf("PPP_InputEvent::HandleInputEvent: instance=%"NACL_PRIu32", "
              "input_event = %"NACL_PRIu32"\n",
              instance, input_event);

  PP_Var character_text = PP_MakeUndefined();
  InputEventData data;
  data.event_type = PPBInputEventInterface()->GetType(input_event);
  data.event_time_stamp = PPBInputEventInterface()->GetTimeStamp(input_event);
  data.event_modifiers = PPBInputEventInterface()->GetModifiers(input_event);

  switch (data.event_type) {
    // These events all use the PPB_MouseInputEvent interface.
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      data.mouse_button =
          PPBMouseInputEventInterface()->GetButton(input_event);
      data.mouse_position =
          PPBMouseInputEventInterface()->GetPosition(input_event);
      data.mouse_click_count =
          PPBMouseInputEventInterface()->GetClickCount(input_event);
      data.mouse_movement =
          PPBMouseInputEventInterface()->GetMovement(input_event);
      break;
    // This event uses the PPB_WheelInputEvent interface.
    case PP_INPUTEVENT_TYPE_WHEEL:
      data.wheel_delta =
          PPBWheelInputEventInterface()->GetDelta(input_event);
      data.wheel_ticks =
          PPBWheelInputEventInterface()->GetTicks(input_event);
      data.wheel_scroll_by_page =
          PPBWheelInputEventInterface()->GetScrollByPage(input_event);
      break;
    // These events all use the PPB_KeyInputEvent interface.
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
    case PP_INPUTEVENT_TYPE_CHAR:
      data.key_code =
          PPBKeyboardInputEventInterface()->GetKeyCode(input_event);
      character_text =
          PPBKeyboardInputEventInterface()->GetCharacterText(input_event);
      break;
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return PP_FALSE;
    // No default case; if any new types are added we should get a compile
    // warning.
  }
  // Now data and character_text have all the data we want to send to the
  // untrusted side.

  // character_text should either be undefined or a string type.
  DCHECK((character_text.type == PP_VARTYPE_UNDEFINED) ||
         (character_text.type == PP_VARTYPE_STRING));
  // Serialize the character_text Var.
  uint32_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(Serialize(&character_text, 1,
                                                &text_size));
  int32_t handled;
  NaClSrpcError srpc_result =
      PppInputEventRpcClient::PPP_InputEvent_HandleInputEvent(
          GetMainSrpcChannel(instance),
          instance,
          input_event,
          sizeof(data),
          reinterpret_cast<char*>(&data),
          text_size,
          text_bytes.get(),
          &handled);
  DebugPrintf("PPP_Instance::HandleInputEvent: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result != NACL_SRPC_RESULT_OK) {
    return PP_FALSE;
  }
  // The 'handled' int should only ever have a value matching one of PP_FALSE
  // or PP_TRUE. Otherwise, there's an error in the proxy.
  DCHECK((handled == static_cast<int32_t>(PP_FALSE) ||
         (handled == static_cast<int32_t>(PP_TRUE))));
  PP_Bool handled_bool = static_cast<PP_Bool>(handled);
  return handled_bool;
}

}  // namespace

const PPP_InputEvent* BrowserInputEvent::GetInterface() {
  static const PPP_InputEvent input_event_interface = {
    HandleInputEvent
  };
  return &input_event_interface;
}

}  // namespace ppapi_proxy
