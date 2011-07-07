// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_INPUT_EVENT_IMPL_H_
#define PPAPI_SHARED_IMPL_INPUT_EVENT_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/thunk/ppb_input_event_api.h"

namespace ppapi {

// IF YOU ADD STUFF TO THIS CLASS
// ==============================
// Be sure to add it to the STRUCT_TRAITS at the top of ppapi_messages.h
struct InputEventData {
  InputEventData();
  ~InputEventData();

  // Internal-only value. Set to true when this input event is filtered, that
  // is, should be delivered synchronously. This is used by the proxy.
  bool is_filtered;

  PP_InputEvent_Type event_type;
  PP_TimeTicks event_time_stamp;
  uint32_t event_modifiers;

  PP_InputEvent_MouseButton mouse_button;
  PP_Point mouse_position;
  int32_t mouse_click_count;

  PP_FloatPoint wheel_delta;
  PP_FloatPoint wheel_ticks;
  bool wheel_scroll_by_page;

  uint32_t key_code;

  std::string character_text;
};

// This simple class implements the PPB_InputEvent_API in terms of the
// shared InputEventData structure
class InputEventImpl : public thunk::PPB_InputEvent_API {
 public:
  explicit InputEventImpl(const InputEventData& data);

  // PPB_InputEvent_API implementation.
  virtual const InputEventData& GetInputEventData() const OVERRIDE;
  virtual PP_InputEvent_Type GetType() OVERRIDE;
  virtual PP_TimeTicks GetTimeStamp() OVERRIDE;
  virtual uint32_t GetModifiers() OVERRIDE;
  virtual PP_InputEvent_MouseButton GetMouseButton() OVERRIDE;
  virtual PP_Point GetMousePosition() OVERRIDE;
  virtual int32_t GetMouseClickCount() OVERRIDE;
  virtual PP_FloatPoint GetWheelDelta() OVERRIDE;
  virtual PP_FloatPoint GetWheelTicks() OVERRIDE;
  virtual PP_Bool GetWheelScrollByPage() OVERRIDE;
  virtual uint32_t GetKeyCode() OVERRIDE;
  virtual PP_Var GetCharacterText() OVERRIDE;

 protected:
  // Derived classes override this to convert a string to a PP_Var in either
  // the proxy or the impl.
  virtual PP_Var StringToPPVar(const std::string& str) = 0;

 private:
  InputEventData data_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_INPUT_EVENT_IMPL_H_

