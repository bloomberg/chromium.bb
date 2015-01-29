// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <algorithm>
#include <linux/input.h>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
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

// Return the values in an array in one string. Used for touch logging.
template <typename T>
std::string DumpArrayProperty(const std::vector<T>& value, const char* format) {
  std::string ret;
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0)
      ret.append(", ");
    ret.append(base::StringPrintf(format, value[i]));
  }
  return ret;
}

// Return the values in a gesture property in one string. Used for touch
// logging.
std::string DumpGesturePropertyValue(GesturesProp* property) {
  switch (property->type()) {
    case GesturePropertyProvider::PT_INT:
      return DumpArrayProperty(property->GetIntValue(), "%d");
      break;
    case GesturePropertyProvider::PT_SHORT:
      return DumpArrayProperty(property->GetShortValue(), "%d");
      break;
    case GesturePropertyProvider::PT_BOOL:
      return DumpArrayProperty(property->GetBoolValue(), "%d");
      break;
    case GesturePropertyProvider::PT_STRING:
      return "\"" + property->GetStringValue() + "\"";
      break;
    case GesturePropertyProvider::PT_REAL:
      return DumpArrayProperty(property->GetDoubleValue(), "%lf");
      break;
    default:
      NOTREACHED();
      break;
  }
  return std::string();
}

// Dump touch device property values to a string.
void DumpTouchDeviceStatus(InputDeviceFactoryEvdev* event_factory,
                           GesturePropertyProvider* provider,
                           std::string* status) {
  // We use DT_ALL since we want gesture property values for all devices that
  // run with the gesture library, not just mice or touchpads.
  std::vector<int> ids;
  event_factory->GetDeviceIdsByType(DT_ALL, &ids);

  // Dump the property names and values for each device.
  for (size_t i = 0; i < ids.size(); ++i) {
    std::vector<std::string> names = provider->GetPropertyNamesById(ids[i]);
    status->append("\n");
    status->append(base::StringPrintf("ID %d:\n", ids[i]));
    status->append(base::StringPrintf(
        "Device \'%s\':\n", provider->GetDeviceNameById(ids[i]).c_str()));

    // Note that, unlike X11, we don't maintain the "atom" concept here.
    // Therefore, the property name indices we output here shouldn't be treated
    // as unique identifiers of the properties.
    std::sort(names.begin(), names.end());
    for (size_t j = 0; j < names.size(); ++j) {
      status->append(base::StringPrintf("\t%s (%zu):", names[j].c_str(), j));
      GesturesProp* property = provider->GetProperty(ids[i], names[j]);
      status->append("\t" + DumpGesturePropertyValue(property) + '\n');
    }
  }
}

#endif

}  // namespace

InputControllerEvdev::InputControllerEvdev(
    KeyboardEvdev* keyboard,
    MouseButtonMapEvdev* button_map
#if defined(USE_EVDEV_GESTURES)
    ,
    GesturePropertyProvider* gesture_property_provider
#endif
    )
    : input_device_factory_(nullptr),
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

void InputControllerEvdev::SetInputDeviceFactory(
    InputDeviceFactoryEvdev* input_device_factory) {
  input_device_factory_ = input_device_factory;
}

bool InputControllerEvdev::HasMouse() {
  if (!input_device_factory_)
    return false;
  return input_device_factory_->GetDeviceIdsByType(DT_MOUSE, NULL);
}

bool InputControllerEvdev::HasTouchpad() {
  if (!input_device_factory_)
    return false;
  return input_device_factory_->GetDeviceIdsByType(DT_TOUCHPAD, NULL);
}

bool InputControllerEvdev::IsCapsLockEnabled() {
  return keyboard_->IsCapsLockEnabled();
}

void InputControllerEvdev::SetCapsLockEnabled(bool enabled) {
  keyboard_->SetCapsLockEnabled(enabled);
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

void InputControllerEvdev::DisableInternalTouchpad() {
  if (input_device_factory_)
    input_device_factory_->DisableInternalTouchpad();
}

void InputControllerEvdev::EnableInternalTouchpad() {
  if (input_device_factory_)
    input_device_factory_->EnableInternalTouchpad();
}

void InputControllerEvdev::DisableInternalKeyboardExceptKeys(
    scoped_ptr<std::set<DomCode>> excepted_keys) {
  if (input_device_factory_) {
    input_device_factory_->DisableInternalKeyboardExceptKeys(
        excepted_keys.Pass());
  }
}

void InputControllerEvdev::EnableInternalKeyboard() {
  if (input_device_factory_)
    input_device_factory_->EnableInternalKeyboard();
}

void InputControllerEvdev::SetIntPropertyForOneType(const EventDeviceType type,
                                                    const std::string& name,
                                                    int value) {
  if (!input_device_factory_)
    return;
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  input_device_factory_->GetDeviceIdsByType(type, &ids);
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
  if (!input_device_factory_)
    return;
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  input_device_factory_->GetDeviceIdsByType(type, &ids);
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

void InputControllerEvdev::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  scoped_ptr<std::string> status(new std::string);
#if defined(USE_EVDEV_GESTURES)
  std::string* status_ptr = status.get();
  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE, base::Bind(&DumpTouchDeviceStatus, input_device_factory_,
                            gesture_property_provider_, status_ptr),
      base::Bind(reply, base::Passed(&status)));
#else
  reply.Run(status.Pass());
#endif
}

}  // namespace ui
