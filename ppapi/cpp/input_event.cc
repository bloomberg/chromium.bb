// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/input_event.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_InputEvent>() {
  return PPB_INPUT_EVENT_INTERFACE;
}

template <> const char* interface_name<PPB_KeyboardInputEvent>() {
  return PPB_KEYBOARD_INPUT_EVENT_INTERFACE;
}

template <> const char* interface_name<PPB_MouseInputEvent>() {
  return PPB_MOUSE_INPUT_EVENT_INTERFACE;
}

template <> const char* interface_name<PPB_WheelInputEvent>() {
  return PPB_WHEEL_INPUT_EVENT_INTERFACE;
}

}  // namespace

// InputEvent ------------------------------------------------------------------

InputEvent::InputEvent() : Resource() {
}

InputEvent::InputEvent(PP_Resource input_event_resource) : Resource() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_InputEvent>())
    return;
  if (get_interface<PPB_InputEvent>()->IsInputEvent(input_event_resource)) {
    Module::Get()->core()->AddRefResource(input_event_resource);
    PassRefFromConstructor(input_event_resource);
  }
}

InputEvent::~InputEvent() {
}

PP_InputEvent_Type InputEvent::GetType() const {
  if (!has_interface<PPB_InputEvent>())
    return PP_INPUTEVENT_TYPE_UNDEFINED;
  return get_interface<PPB_InputEvent>()->GetType(pp_resource());
}

PP_TimeTicks InputEvent::GetTimeStamp() const {
  if (!has_interface<PPB_InputEvent>())
    return 0.0f;
  return get_interface<PPB_InputEvent>()->GetTimeStamp(pp_resource());
}

uint32_t InputEvent::GetModifiers() const {
  if (!has_interface<PPB_InputEvent>())
    return 0;
  return get_interface<PPB_InputEvent>()->GetModifiers(pp_resource());
}

// MouseInputEvent -------------------------------------------------------------

MouseInputEvent::MouseInputEvent() : InputEvent() {
}

MouseInputEvent::MouseInputEvent(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_MouseInputEvent>())
    return;
  if (get_interface<PPB_MouseInputEvent>()->IsMouseInputEvent(
          event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

MouseInputEvent::MouseInputEvent(Instance* instance,
                                 PP_InputEvent_Type type,
                                 PP_TimeTicks time_stamp,
                                 uint32_t modifiers,
                                 PP_InputEvent_MouseButton mouse_button,
                                 const Point& mouse_position,
                                 int32_t click_count,
                                 const Point& mouse_movement) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_MouseInputEvent>())
    return;
  PassRefFromConstructor(get_interface<PPB_MouseInputEvent>()->Create(
      instance->pp_instance(), type, time_stamp, modifiers, mouse_button,
      &mouse_position.pp_point(), click_count, &mouse_movement.pp_point()));
}

PP_InputEvent_MouseButton MouseInputEvent::GetButton() const {
  if (!has_interface<PPB_MouseInputEvent>())
    return PP_INPUTEVENT_MOUSEBUTTON_NONE;
  return get_interface<PPB_MouseInputEvent>()->GetButton(pp_resource());
}

Point MouseInputEvent::GetPosition() const {
  if (!has_interface<PPB_MouseInputEvent>())
    return Point();
  return get_interface<PPB_MouseInputEvent>()->GetPosition(pp_resource());
}

int32_t MouseInputEvent::GetClickCount() const {
  if (!has_interface<PPB_MouseInputEvent>())
    return 0;
  return get_interface<PPB_MouseInputEvent>()->GetClickCount(pp_resource());
}

Point MouseInputEvent::GetMovement() const {
  if (!has_interface<PPB_MouseInputEvent>())
    return Point();
  return get_interface<PPB_MouseInputEvent>()->GetMovement(pp_resource());
}

// WheelInputEvent -------------------------------------------------------------

WheelInputEvent::WheelInputEvent() : InputEvent() {
}

WheelInputEvent::WheelInputEvent(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_WheelInputEvent>())
    return;
  if (get_interface<PPB_WheelInputEvent>()->IsWheelInputEvent(
          event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

WheelInputEvent::WheelInputEvent(Instance* instance,
                                 PP_TimeTicks time_stamp,
                                 uint32_t modifiers,
                                 const FloatPoint& wheel_delta,
                                 const FloatPoint& wheel_ticks,
                                 bool scroll_by_page) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_WheelInputEvent>())
    return;
  PassRefFromConstructor(get_interface<PPB_WheelInputEvent>()->Create(
      instance->pp_instance(), time_stamp, modifiers,
      &wheel_delta.pp_float_point(), &wheel_ticks.pp_float_point(),
      PP_FromBool(scroll_by_page)));
}

FloatPoint WheelInputEvent::GetDelta() const {
  if (!has_interface<PPB_WheelInputEvent>())
    return FloatPoint();
  return get_interface<PPB_WheelInputEvent>()->GetDelta(pp_resource());
}

FloatPoint WheelInputEvent::GetTicks() const {
  if (!has_interface<PPB_WheelInputEvent>())
    return FloatPoint();
  return get_interface<PPB_WheelInputEvent>()->GetTicks(pp_resource());
}

bool WheelInputEvent::GetScrollByPage() const {
  if (!has_interface<PPB_WheelInputEvent>())
    return false;
  return PP_ToBool(
      get_interface<PPB_WheelInputEvent>()->GetScrollByPage(pp_resource()));
}

// KeyboardInputEvent ----------------------------------------------------------

KeyboardInputEvent::KeyboardInputEvent() : InputEvent() {
}

KeyboardInputEvent::KeyboardInputEvent(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_KeyboardInputEvent>())
    return;
  if (get_interface<PPB_KeyboardInputEvent>()->IsKeyboardInputEvent(
          event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

KeyboardInputEvent::KeyboardInputEvent(Instance* instance,
                                       PP_InputEvent_Type type,
                                       PP_TimeTicks time_stamp,
                                       uint32_t modifiers,
                                       uint32_t key_code,
                                       const Var& character_text) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_KeyboardInputEvent>())
    return;
  PassRefFromConstructor(get_interface<PPB_KeyboardInputEvent>()->Create(
      instance->pp_instance(), type, time_stamp, modifiers, key_code,
      character_text.pp_var()));
}

uint32_t KeyboardInputEvent::GetKeyCode() const {
  if (!has_interface<PPB_KeyboardInputEvent>())
    return 0;
  return get_interface<PPB_KeyboardInputEvent>()->GetKeyCode(pp_resource());
}

Var KeyboardInputEvent::GetCharacterText() const {
  if (!has_interface<PPB_KeyboardInputEvent>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_KeyboardInputEvent>()->GetCharacterText(
                 pp_resource()));
}

}  // namespace pp
