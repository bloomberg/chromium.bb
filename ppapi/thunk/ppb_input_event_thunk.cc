// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_input_event_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_InputEvent_API> EnterInputEvent;

// InputEvent ------------------------------------------------------------------

int32_t RequestInputEvents(PP_Instance instance, uint32_t event_classes) {
  EnterInstance enter(instance);
  if (enter.failed())
    return enter.retval();
  return enter.functions()->RequestInputEvents(instance, event_classes);
}

int32_t RequestFilteringInputEvents(PP_Instance instance,
                                    uint32_t event_classes) {
  EnterInstance enter(instance);
  if (enter.failed())
    return enter.retval();
  return enter.functions()->RequestFilteringInputEvents(instance,
                                                        event_classes);
}

void ClearInputEventRequest(PP_Instance instance,
                            uint32_t event_classes) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->ClearInputEventRequest(instance, event_classes);
}

PP_Bool IsInputEvent(PP_Resource resource) {
  EnterInputEvent enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_InputEvent_Type GetType(PP_Resource event) {
  EnterInputEvent enter(event, true);
  if (enter.failed())
    return PP_INPUTEVENT_TYPE_UNDEFINED;
  return enter.object()->GetType();
}

PP_TimeTicks GetTimeStamp(PP_Resource event) {
  EnterInputEvent enter(event, true);
  if (enter.failed())
    return 0.0;
  return enter.object()->GetTimeStamp();
}

uint32_t GetModifiers(PP_Resource event) {
  EnterInputEvent enter(event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetModifiers();
}

const PPB_InputEvent g_ppb_input_event_thunk = {
  &RequestInputEvents,
  &RequestFilteringInputEvents,
  &ClearInputEventRequest,
  &IsInputEvent,
  &GetType,
  &GetTimeStamp,
  &GetModifiers
};

// Mouse -----------------------------------------------------------------------

PP_Resource CreateMouseInputEvent1_0(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     PP_InputEvent_MouseButton mouse_button,
                                     const PP_Point* mouse_position,
                                     int32_t click_count) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;

  PP_Point mouse_movement = PP_MakePoint(0, 0);
  return enter.functions()->CreateMouseInputEvent(instance, type, time_stamp,
                                                  modifiers, mouse_button,
                                                  mouse_position, click_count,
                                                  &mouse_movement);
}

PP_Resource CreateMouseInputEvent1_1(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     PP_InputEvent_MouseButton mouse_button,
                                     const PP_Point* mouse_position,
                                     int32_t click_count,
                                     const PP_Point* mouse_movement) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateMouseInputEvent(instance, type, time_stamp,
                                                  modifiers, mouse_button,
                                                  mouse_position, click_count,
                                                  mouse_movement);
}

PP_Bool IsMouseInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;  // Prevent warning log in GetType.
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
                     type == PP_INPUTEVENT_TYPE_MOUSEUP ||
                     type == PP_INPUTEVENT_TYPE_MOUSEMOVE ||
                     type == PP_INPUTEVENT_TYPE_MOUSEENTER ||
                     type == PP_INPUTEVENT_TYPE_MOUSELEAVE ||
                     type == PP_INPUTEVENT_TYPE_CONTEXTMENU);
}

PP_InputEvent_MouseButton GetMouseButton(PP_Resource mouse_event) {
  EnterInputEvent enter(mouse_event, true);
  if (enter.failed())
    return PP_INPUTEVENT_MOUSEBUTTON_NONE;
  return enter.object()->GetMouseButton();
}

PP_Point GetMousePosition(PP_Resource mouse_event) {
  EnterInputEvent enter(mouse_event, true);
  if (enter.failed())
    return PP_MakePoint(0, 0);
  return enter.object()->GetMousePosition();
}

int32_t GetMouseClickCount(PP_Resource mouse_event) {
  EnterInputEvent enter(mouse_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetMouseClickCount();
}

PP_Point GetMouseMovement(PP_Resource mouse_event) {
  EnterInputEvent enter(mouse_event, true);
  if (enter.failed())
    return PP_MakePoint(0, 0);
  return enter.object()->GetMouseMovement();
}

const PPB_MouseInputEvent_1_0 g_ppb_mouse_input_event_1_0_thunk = {
  &CreateMouseInputEvent1_0,
  &IsMouseInputEvent,
  &GetMouseButton,
  &GetMousePosition,
  &GetMouseClickCount
};

const PPB_MouseInputEvent g_ppb_mouse_input_event_1_1_thunk = {
  &CreateMouseInputEvent1_1,
  &IsMouseInputEvent,
  &GetMouseButton,
  &GetMousePosition,
  &GetMouseClickCount,
  &GetMouseMovement
};

// Wheel -----------------------------------------------------------------------

PP_Resource CreateWheelInputEvent(PP_Instance instance,
                                  PP_TimeTicks time_stamp,
                                  uint32_t modifiers,
                                  const PP_FloatPoint* wheel_delta,
                                  const PP_FloatPoint* wheel_ticks,
                                  PP_Bool scroll_by_page) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateWheelInputEvent(instance, time_stamp,
                                                  modifiers, wheel_delta,
                                                  wheel_ticks, scroll_by_page);
}

PP_Bool IsWheelInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;  // Prevent warning log in GetType.
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_WHEEL);
}

PP_FloatPoint GetWheelDelta(PP_Resource wheel_event) {
  EnterInputEvent enter(wheel_event, true);
  if (enter.failed())
    return PP_MakeFloatPoint(0.0f, 0.0f);
  return enter.object()->GetWheelDelta();
}

PP_FloatPoint GetWheelTicks(PP_Resource wheel_event) {
  EnterInputEvent enter(wheel_event, true);
  if (enter.failed())
    return PP_MakeFloatPoint(0.0f, 0.0f);
  return enter.object()->GetWheelTicks();
}

PP_Bool GetWheelScrollByPage(PP_Resource wheel_event) {
  EnterInputEvent enter(wheel_event, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetWheelScrollByPage();
}

const PPB_WheelInputEvent g_ppb_wheel_input_event_thunk = {
  &CreateWheelInputEvent,
  &IsWheelInputEvent,
  &GetWheelDelta,
  &GetWheelTicks,
  &GetWheelScrollByPage
};

// Keyboard --------------------------------------------------------------------

PP_Resource CreateKeyboardInputEvent(PP_Instance instance,
                                     PP_InputEvent_Type type,
                                     PP_TimeTicks time_stamp,
                                     uint32_t modifiers,
                                     uint32_t key_code,
                                     struct PP_Var character_text) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateKeyboardInputEvent(instance, type, time_stamp,
                                                     modifiers, key_code,
                                                     character_text);
}

PP_Bool IsKeyboardInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;  // Prevent warning log in GetType.
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_KEYDOWN ||
                     type == PP_INPUTEVENT_TYPE_KEYUP ||
                     type == PP_INPUTEVENT_TYPE_RAWKEYDOWN ||
                     type == PP_INPUTEVENT_TYPE_CHAR);
}

uint32_t GetKeyCode(PP_Resource key_event) {
  EnterInputEvent enter(key_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetKeyCode();
}

PP_Var GetCharacterText(PP_Resource character_event) {
  EnterInputEvent enter(character_event, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetCharacterText();
}

const PPB_KeyboardInputEvent g_ppb_keyboard_input_event_thunk = {
  &CreateKeyboardInputEvent,
  &IsKeyboardInputEvent,
  &GetKeyCode,
  &GetCharacterText
};

// _Dev interface.

PP_Bool SetUsbKeyCode(PP_Resource key_event, uint32_t usb_key_code) {
  EnterInputEvent enter(key_event, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetUsbKeyCode(usb_key_code);
}

uint32_t GetUsbKeyCode(PP_Resource key_event) {
  EnterInputEvent enter(key_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetUsbKeyCode();
}

const PPB_KeyboardInputEvent_Dev g_ppb_keyboard_input_event_dev_thunk = {
  &SetUsbKeyCode,
  &GetUsbKeyCode,
};

// Composition -----------------------------------------------------------------

PP_Resource CreateIMEInputEvent(PP_Instance instance,
                                PP_InputEvent_Type type,
                                PP_TimeTicks time_stamp,
                                PP_Var text,
                                uint32_t segment_number,
                                const uint32_t segment_offsets[],
                                int32_t target_segment,
                                uint32_t selection_start,
                                uint32_t selection_end) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateIMEInputEvent(instance, type, time_stamp,
                                                text, segment_number,
                                                segment_offsets,
                                                target_segment,
                                                selection_start,
                                                selection_end);
}

PP_Bool IsIMEInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;  // Prevent warning log in GetType.
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_IME_COMPOSITION_START ||
                     type == PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE ||
                     type == PP_INPUTEVENT_TYPE_IME_COMPOSITION_END ||
                     type == PP_INPUTEVENT_TYPE_IME_TEXT);
}

PP_Var GetIMEText(PP_Resource ime_event) {
  return GetCharacterText(ime_event);
}

uint32_t GetIMESegmentNumber(PP_Resource ime_event) {
  EnterInputEvent enter(ime_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetIMESegmentNumber();
}

uint32_t GetIMESegmentOffset(PP_Resource ime_event, uint32_t index) {
  EnterInputEvent enter(ime_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetIMESegmentOffset(index);
}

int32_t GetIMETargetSegment(PP_Resource ime_event) {
  EnterInputEvent enter(ime_event, true);
  if (enter.failed())
    return -1;
  return enter.object()->GetIMETargetSegment();
}

void GetIMESelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  EnterInputEvent enter(ime_event, true);
  if (enter.failed()) {
    if (start)
      *start = 0;
    if (end)
      *end = 0;
    return;
  }
  enter.object()->GetIMESelection(start, end);
}

const PPB_IMEInputEvent_Dev_0_1 g_ppb_ime_input_event_0_1_thunk = {
  &IsIMEInputEvent,
  &GetIMEText,
  &GetIMESegmentNumber,
  &GetIMESegmentOffset,
  &GetIMETargetSegment,
  &GetIMESelection
};

const PPB_IMEInputEvent_Dev_0_2 g_ppb_ime_input_event_0_2_thunk = {
  &CreateIMEInputEvent,
  &IsIMEInputEvent,
  &GetIMEText,
  &GetIMESegmentNumber,
  &GetIMESegmentOffset,
  &GetIMETargetSegment,
  &GetIMESelection
};

// Touch -----------------------------------------------------------------------

PP_Resource CreateTouchInputEvent(PP_Instance instance,
                                  PP_InputEvent_Type type,
                                  PP_TimeTicks time_stamp,
                                  uint32_t modifiers) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTouchInputEvent(instance, type, time_stamp,
                                                  modifiers);
}

void AddTouchPoint(PP_Resource touch_event,
                   PP_TouchListType list,
                   const PP_TouchPoint* point) {
  EnterInputEvent enter(touch_event, true);
  if (enter.failed())
    return;
  return enter.object()->AddTouchPoint(list, *point);
}

PP_Bool IsTouchInputEvent(PP_Resource resource) {
  if (!IsInputEvent(resource))
    return PP_FALSE;  // Prevent warning log in GetType.
  PP_InputEvent_Type type = GetType(resource);
  return PP_FromBool(type == PP_INPUTEVENT_TYPE_TOUCHSTART ||
                     type == PP_INPUTEVENT_TYPE_TOUCHMOVE ||
                     type == PP_INPUTEVENT_TYPE_TOUCHEND ||
                     type == PP_INPUTEVENT_TYPE_TOUCHCANCEL);
}

uint32_t GetTouchCount(PP_Resource touch_event, PP_TouchListType list) {
  EnterInputEvent enter(touch_event, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetTouchCount(list);
}

struct PP_TouchPoint GetTouchByIndex(PP_Resource touch_event,
                                     PP_TouchListType list,
                                     uint32_t index) {
  EnterInputEvent enter(touch_event, true);
  if (enter.failed())
    return PP_MakeTouchPoint();
  return enter.object()->GetTouchByIndex(list, index);
}

struct PP_TouchPoint GetTouchById(PP_Resource touch_event,
                                  PP_TouchListType list,
                                  uint32_t id) {
  EnterInputEvent enter(touch_event, true);
  if (enter.failed())
    return PP_MakeTouchPoint();
  return enter.object()->GetTouchById(list, id);
}

const PPB_TouchInputEvent_1_0 g_ppb_touch_input_event_thunk = {
  &CreateTouchInputEvent,
  &AddTouchPoint,
  &IsTouchInputEvent,
  &GetTouchCount,
  &GetTouchByIndex,
  &GetTouchById
};

}  // namespace

const PPB_InputEvent_1_0* GetPPB_InputEvent_1_0_Thunk() {
  return &g_ppb_input_event_thunk;
}

const PPB_MouseInputEvent_1_0* GetPPB_MouseInputEvent_1_0_Thunk() {
  return &g_ppb_mouse_input_event_1_0_thunk;
}

const PPB_MouseInputEvent_1_1* GetPPB_MouseInputEvent_1_1_Thunk() {
  return &g_ppb_mouse_input_event_1_1_thunk;
}

const PPB_KeyboardInputEvent_1_0* GetPPB_KeyboardInputEvent_1_0_Thunk() {
  return &g_ppb_keyboard_input_event_thunk;
}

const PPB_KeyboardInputEvent_Dev_0_1*
    GetPPB_KeyboardInputEvent_Dev_0_1_Thunk() {
  return &g_ppb_keyboard_input_event_dev_thunk;
}

const PPB_WheelInputEvent_1_0* GetPPB_WheelInputEvent_1_0_Thunk() {
  return &g_ppb_wheel_input_event_thunk;
}

const PPB_IMEInputEvent_Dev_0_1* GetPPB_IMEInputEvent_Dev_0_1_Thunk() {
  return &g_ppb_ime_input_event_0_1_thunk;
}

const PPB_IMEInputEvent_Dev_0_2* GetPPB_IMEInputEvent_Dev_0_2_Thunk() {
  return &g_ppb_ime_input_event_0_2_thunk;
}

const PPB_TouchInputEvent_1_0* GetPPB_TouchInputEvent_Thunk() {
  return &g_ppb_touch_input_event_thunk;
}

}  // namespace thunk
}  // namespace ppapi
