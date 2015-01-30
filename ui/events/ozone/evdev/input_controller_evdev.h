// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_

#include <string>

#include "base/basictypes.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

class InputDeviceFactoryEvdev;
class KeyboardEvdev;
class MouseButtonMapEvdev;

// Ozone InputController implementation for the Linux input subsystem ("evdev").
class EVENTS_OZONE_EVDEV_EXPORT InputControllerEvdev : public InputController {
 public:
  InputControllerEvdev(KeyboardEvdev* keyboard,
                       MouseButtonMapEvdev* button_map);
  ~InputControllerEvdev() override;

  // Initialize device factory. This would be in the constructor if it was
  // built early enough for that to be possible.
  void SetInputDeviceFactory(InputDeviceFactoryEvdev* input_device_factory);

  void set_has_mouse(bool has_mouse);
  void set_has_touchpad(bool has_touchpad);

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
  void SetTapToClickPaused(bool state) override;
  void GetTouchDeviceStatus(const GetTouchDeviceStatusReply& reply) override;
  void DisableInternalTouchpad() override;
  void EnableInternalTouchpad() override;
  void DisableInternalKeyboardExceptKeys(
      scoped_ptr<std::set<DomCode>> excepted_keys) override;
  void EnableInternalKeyboard() override;

 private:
  // Factory for devices. Needed to update device config.
  InputDeviceFactoryEvdev* input_device_factory_;

  // Keyboard state.
  KeyboardEvdev* keyboard_;

  // Mouse button map.
  MouseButtonMapEvdev* button_map_;

  // Device presence.
  bool has_mouse_;
  bool has_touchpad_;

  DISALLOW_COPY_AND_ASSIGN(InputControllerEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
