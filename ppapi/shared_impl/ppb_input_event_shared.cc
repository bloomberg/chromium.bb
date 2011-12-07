// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_input_event_shared.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"

using ppapi::thunk::PPB_InputEvent_API;

namespace ppapi {

InputEventData::InputEventData()
    : is_filtered(false),
      event_type(PP_INPUTEVENT_TYPE_UNDEFINED),
      event_time_stamp(0.0),
      event_modifiers(0),
      mouse_button(PP_INPUTEVENT_MOUSEBUTTON_NONE),
      mouse_position(PP_MakePoint(0, 0)),
      mouse_click_count(0),
      mouse_movement(PP_MakePoint(0, 0)),
      wheel_delta(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_ticks(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_scroll_by_page(false),
      key_code(0),
      character_text(),
      composition_target_segment(-1),
      composition_selection_start(0),
      composition_selection_end(0) {
}

InputEventData::~InputEventData() {
}

PPB_InputEvent_Shared::PPB_InputEvent_Shared(const InitAsImpl&,
                                             PP_Instance instance,
                                             const InputEventData& data)
    : Resource(instance),
      data_(data) {
}

PPB_InputEvent_Shared::PPB_InputEvent_Shared(const InitAsProxy&,
                                             PP_Instance instance,
                                             const InputEventData& data)
    : Resource(HostResource::MakeInstanceOnly(instance)),
      data_(data) {
}

PPB_InputEvent_API* PPB_InputEvent_Shared::AsPPB_InputEvent_API() {
  return this;
}

const InputEventData& PPB_InputEvent_Shared::GetInputEventData() const {
  return data_;
}

PP_InputEvent_Type PPB_InputEvent_Shared::GetType() {
  return data_.event_type;
}

PP_TimeTicks PPB_InputEvent_Shared::GetTimeStamp() {
  return data_.event_time_stamp;
}

uint32_t PPB_InputEvent_Shared::GetModifiers() {
  return data_.event_modifiers;
}

PP_InputEvent_MouseButton PPB_InputEvent_Shared::GetMouseButton() {
  return data_.mouse_button;
}

PP_Point PPB_InputEvent_Shared::GetMousePosition() {
  return data_.mouse_position;
}

int32_t PPB_InputEvent_Shared::GetMouseClickCount() {
  return data_.mouse_click_count;
}

PP_Point PPB_InputEvent_Shared::GetMouseMovement() {
  return data_.mouse_movement;
}

PP_FloatPoint PPB_InputEvent_Shared::GetWheelDelta() {
  return data_.wheel_delta;
}

PP_FloatPoint PPB_InputEvent_Shared::GetWheelTicks() {
  return data_.wheel_ticks;
}

PP_Bool PPB_InputEvent_Shared::GetWheelScrollByPage() {
  return PP_FromBool(data_.wheel_scroll_by_page);
}

uint32_t PPB_InputEvent_Shared::GetKeyCode() {
  return data_.key_code;
}

PP_Var PPB_InputEvent_Shared::GetCharacterText() {
  return StringVar::StringToPPVar(
      PpapiGlobals::Get()->GetModuleForInstance(pp_instance()),
      data_.character_text);
}

uint32_t PPB_InputEvent_Shared::GetIMESegmentNumber() {
  if (data_.composition_segment_offsets.empty())
    return 0;
  return data_.composition_segment_offsets.size() - 1;
}

uint32_t PPB_InputEvent_Shared::GetIMESegmentOffset(uint32_t index) {
  if (index >= data_.composition_segment_offsets.size())
    return 0;
  return data_.composition_segment_offsets[index];
}

int32_t PPB_InputEvent_Shared::GetIMETargetSegment() {
  return data_.composition_target_segment;
}

void PPB_InputEvent_Shared::GetIMESelection(uint32_t* start, uint32_t* end) {
  if (start)
    *start = data_.composition_selection_start;
  if (end)
    *end = data_.composition_selection_end;
}

}  // namespace ppapi
