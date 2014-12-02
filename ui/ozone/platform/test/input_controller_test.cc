// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/input_controller_test.h"

#include "base/compiler_specific.h"

namespace ui {

InputControllerTest::InputControllerTest() {
}

InputControllerTest::~InputControllerTest() {
}

bool InputControllerTest::HasMouse() {
  return false;
}

bool InputControllerTest::HasTouchpad() {
  return false;
}

void InputControllerTest::SetTouchpadSensitivity(int value) {
  ALLOW_UNUSED_LOCAL(value);
}

void InputControllerTest::SetTapToClick(bool enabled) {
  ALLOW_UNUSED_LOCAL(enabled);
}

void InputControllerTest::SetThreeFingerClick(bool enabled) {
  ALLOW_UNUSED_LOCAL(enabled);
}

void InputControllerTest::SetTapDragging(bool enabled) {
  ALLOW_UNUSED_LOCAL(enabled);
}

void InputControllerTest::SetNaturalScroll(bool enabled) {
  ALLOW_UNUSED_LOCAL(enabled);
}

void InputControllerTest::SetMouseSensitivity(int value) {
  ALLOW_UNUSED_LOCAL(value);
}

void InputControllerTest::SetPrimaryButtonRight(bool right) {
  ALLOW_UNUSED_LOCAL(right);
}

}  // namespace ui
