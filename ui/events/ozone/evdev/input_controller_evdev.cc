// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <linux/input.h>
#include <vector>

#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

namespace ui {

namespace {

#if defined(USE_EVDEV_GESTURES)
void SetGestureIntProperty(GesturePropertyProvider* provider,
                           int id,
                           const std::string& name,
                           int value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<int> values(1, value);
    property->SetIntValue(values);
  }
}

void SetGestureBoolProperty(GesturePropertyProvider* provider,
                            int id,
                            const std::string& name,
                            bool value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<bool> values(1, value);
    property->SetBoolValue(values);
  }
}
#endif

}  // namespace

InputControllerEvdev::InputControllerEvdev(
    EventFactoryEvdev* event_factory,
    KeyboardEvdev* keyboard,
    MouseButtonMapEvdev* button_map
#if defined(USE_EVDEV_GESTURES)
    ,
    GesturePropertyProvider* gesture_property_provider
#endif
    )
    : event_factory_(event_factory),
      keyboard_(keyboard),
      button_map_(button_map)
#if defined(USE_EVDEV_GESTURES)
      ,
      gesture_property_provider_(gesture_property_provider)
#endif
{
}

InputControllerEvdev::~InputControllerEvdev() {
}

bool InputControllerEvdev::HasMouse() {
  return event_factory_->GetDeviceIdsByType(DT_MOUSE, NULL);
}

bool InputControllerEvdev::HasTouchpad() {
  return event_factory_->GetDeviceIdsByType(DT_TOUCHPAD, NULL);
}

bool InputControllerEvdev::IsCapsLockEnabled() {
  return false;
}

void InputControllerEvdev::SetCapsLockEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void InputControllerEvdev::SetNumLockEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

bool InputControllerEvdev::IsAutoRepeatEnabled() {
  return keyboard_->IsAutoRepeatEnabled();
}

void InputControllerEvdev::SetAutoRepeatEnabled(bool enabled) {
  keyboard_->SetAutoRepeatEnabled(enabled);
}

void InputControllerEvdev::SetAutoRepeatRate(const base::TimeDelta& delay,
                                             const base::TimeDelta& interval) {
  keyboard_->SetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::GetAutoRepeatRate(base::TimeDelta* delay,
                                             base::TimeDelta* interval) {
  keyboard_->GetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::SetIntPropertyForOneType(const EventDeviceType type,
                                                    const std::string& name,
                                                    int value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  event_factory_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureIntProperty(gesture_property_provider_, ids[i], name, value);
  }
#endif
  // In the future, we may add property setting codes for other non-gesture
  // devices. One example would be keyboard settings.
  // TODO(sheckylin): See http://crbug.com/398518 for example.
}

void InputControllerEvdev::SetBoolPropertyForOneType(const EventDeviceType type,
                                                     const std::string& name,
                                                     bool value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  event_factory_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureBoolProperty(gesture_property_provider_, ids[i], name, value);
  }
#endif
}

void InputControllerEvdev::SetTouchpadSensitivity(int value) {
  SetIntPropertyForOneType(DT_TOUCHPAD, "Pointer Sensitivity", value);
  SetIntPropertyForOneType(DT_TOUCHPAD, "Scroll Sensitivity", value);
}

void InputControllerEvdev::SetTapToClick(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Enable", enabled);
}

void InputControllerEvdev::SetThreeFingerClick(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "T5R2 Three Finger Click Enable",
                            enabled);
}

void InputControllerEvdev::SetTapDragging(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Drag Enable", enabled);
}

void InputControllerEvdev::SetNaturalScroll(bool enabled) {
  SetBoolPropertyForOneType(DT_MULTITOUCH, "Australian Scrolling", enabled);
}

void InputControllerEvdev::SetMouseSensitivity(int value) {
  SetIntPropertyForOneType(DT_MOUSE, "Pointer Sensitivity", value);
  SetIntPropertyForOneType(DT_MOUSE, "Scroll Sensitivity", value);
}

void InputControllerEvdev::SetPrimaryButtonRight(bool right) {
  button_map_->UpdateButtonMap(BTN_LEFT, right ? BTN_RIGHT : BTN_LEFT);
  button_map_->UpdateButtonMap(BTN_RIGHT, right ? BTN_LEFT : BTN_RIGHT);
}

void InputControllerEvdev::SetTapToClickPaused(bool state) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Paused", state);
}

}  // namespace ui
