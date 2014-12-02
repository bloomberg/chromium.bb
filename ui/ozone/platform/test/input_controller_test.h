// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_INPUT_CONTROLLER_TEST_H_
#define UI_OZONE_PLATFORM_TEST_INPUT_CONTROLLER_TEST_H_

#include "base/basictypes.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

// Ozone InputController implementation for test.
class InputControllerTest : public InputController {
 public:
  InputControllerTest();
  virtual ~InputControllerTest();

  // InputController overrides:
  //
  // Interfaces for configuring input devices.
  bool HasMouse() override;
  bool HasTouchpad() override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputControllerTest);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_INPUT_CONTROLLER_TEST_H_
