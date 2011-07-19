// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/input_event_impl.h"

namespace ppapi {

InputEventData::InputEventData()
    : is_filtered(false),
      event_type(PP_INPUTEVENT_TYPE_UNDEFINED),
      event_time_stamp(0.0),
      event_modifiers(0),
      mouse_button(PP_INPUTEVENT_MOUSEBUTTON_NONE),
      mouse_position(PP_MakePoint(0, 0)),
      mouse_click_count(0),
      wheel_delta(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_ticks(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_scroll_by_page(false),
      key_code(0),
      character_text() {
}

InputEventData::~InputEventData() {
}

InputEventImpl::InputEventImpl(const InputEventData& data) : data_(data) {
}

const InputEventData& InputEventImpl::GetInputEventData() const {
  return data_;
}

PP_InputEvent_Type InputEventImpl::GetType() {
  return data_.event_type;
}

PP_TimeTicks InputEventImpl::GetTimeStamp() {
  return data_.event_time_stamp;
}

uint32_t InputEventImpl::GetModifiers() {
  return data_.event_modifiers;
}

PP_InputEvent_MouseButton InputEventImpl::GetMouseButton() {
  return data_.mouse_button;
}

PP_Point InputEventImpl::GetMousePosition() {
  return data_.mouse_position;
}

int32_t InputEventImpl::GetMouseClickCount() {
  return data_.mouse_click_count;
}

PP_FloatPoint InputEventImpl::GetWheelDelta() {
  return data_.wheel_delta;
}

PP_FloatPoint InputEventImpl::GetWheelTicks() {
  return data_.wheel_ticks;
}

PP_Bool InputEventImpl::GetWheelScrollByPage() {
  return PP_FromBool(data_.wheel_scroll_by_page);
}

uint32_t InputEventImpl::GetKeyCode() {
  return data_.key_code;
}

PP_Var InputEventImpl::GetCharacterText() {
  return StringToPPVar(data_.character_text);
}

}  // namespace ppapi

