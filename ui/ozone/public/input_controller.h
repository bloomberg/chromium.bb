// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
#define UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

// Platform-specific interface for controlling input devices.
//
// The object provides methods for the preference page to configure input
// devices w.r.t. the user setting. On ChromeOS, this replaces the inputcontrol
// script that is originally located at /opt/google/chrome/.
class OZONE_EXPORT InputController {
 public:
  InputController() {}
  virtual ~InputController() {}

  // Functions for checking devices existence.
  virtual bool HasMouse() = 0;
  virtual bool HasTouchpad() = 0;

  // Functions for changing device settings.
  //
  // See c/b/c/system/input_device_settings.h for more context.
  virtual void SetTouchpadSensitivity(int value) = 0;
  virtual void SetTapToClick(bool enabled) = 0;
  virtual void SetThreeFingerClick(bool enabled) = 0;
  virtual void SetTapDragging(bool enabled) = 0;
  virtual void SetNaturalScroll(bool enabled) = 0;
  virtual void SetMouseSensitivity(int value) = 0;
  virtual void SetPrimaryButtonRight(bool right) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputController);
};

// Create an input controller that does nothing.
OZONE_EXPORT scoped_ptr<InputController> CreateStubInputController();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
