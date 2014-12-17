// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_

#include <string>

#include "base/basictypes.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

class EventFactoryEvdev;
class KeyboardEvdev;
class MouseButtonMapEvdev;

#if defined(USE_EVDEV_GESTURES)
class GesturePropertyProvider;
#endif

// Ozone InputController implementation for the Linux input subsystem ("evdev").
class EVENTS_OZONE_EVDEV_EXPORT InputControllerEvdev : public InputController {
 public:
  InputControllerEvdev(EventFactoryEvdev* event_factory,
                       KeyboardEvdev* keyboard,
                       MouseButtonMapEvdev* button_map
#if defined(USE_EVDEV_GESTURES)
                       ,
                       GesturePropertyProvider* gesture_property_provider
#endif
                       );
  virtual ~InputControllerEvdev();

  // InputController:
  bool HasMouse() override;
  bool HasTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;

 private:
  // Set a property value for all devices of one type.
  void SetIntPropertyForOneType(const EventDeviceType type,
                                const std::string& name,
                                int value);
  void SetBoolPropertyForOneType(const EventDeviceType type,
                                 const std::string& name,
                                 bool value);

  // Event factory object which manages device event converters.
  EventFactoryEvdev* event_factory_;

  // Keyboard state.
  KeyboardEvdev* keyboard_;

  // Mouse button map.
  MouseButtonMapEvdev* button_map_;

#if defined(USE_EVDEV_GESTURES)
  // Gesture library property provider.
  GesturePropertyProvider* gesture_property_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(InputControllerEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
