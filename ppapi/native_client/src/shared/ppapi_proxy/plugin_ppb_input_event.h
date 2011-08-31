// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INPUT_EVENT_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INPUT_EVENT_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/input_event_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/ppb_input_event.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_InputEvent interface.
class PluginInputEvent : public PluginResource {
 public:
  PluginInputEvent();
  // Init the PluginInputEvent resource with the given data. Assumes
  // character_text has been AddRefed if it's a string, and takes ownership.
  void Init(const InputEventData& input_event_data, PP_Var character_text);
  virtual ~PluginInputEvent();

  // PluginResource implementation.
  virtual bool InitFromBrowserResource(PP_Resource /*resource*/) {
    return true;
  }

  static const PPB_InputEvent* GetInterface();
  static const PPB_MouseInputEvent_1_0* GetMouseInterface1_0();
  static const PPB_MouseInputEvent* GetMouseInterface1_1();
  static const PPB_WheelInputEvent* GetWheelInterface();
  static const PPB_KeyboardInputEvent* GetKeyboardInterface();

  PP_InputEvent_Type GetType() const;
  PP_TimeTicks GetTimeStamp() const;
  uint32_t GetModifiers() const;

  PP_InputEvent_MouseButton GetMouseButton() const;
  PP_Point GetMousePosition() const;
  int32_t GetMouseClickCount() const;
  PP_Point GetMouseMovement() const;

  PP_FloatPoint GetWheelDelta() const;
  PP_FloatPoint GetWheelTicks() const;
  PP_Bool GetWheelScrollByPage() const;

  uint32_t GetKeyCode() const;
  PP_Var GetCharacterText() const;

 private:
  IMPLEMENT_RESOURCE(PluginInputEvent);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInputEvent);

  InputEventData input_event_data_;
  PP_Var character_text_;  // Undefined if this is not a character event.
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INPUT_EVENT_H_
