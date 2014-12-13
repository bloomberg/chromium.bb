// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/input_controller.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace ui {

namespace {

class StubInputController : public InputController {
 public:
  StubInputController();
  virtual ~StubInputController();

  // InputController:
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
  DISALLOW_COPY_AND_ASSIGN(StubInputController);
};

StubInputController::StubInputController() {
}

StubInputController::~StubInputController() {
}

bool StubInputController::HasMouse() {
  NOTIMPLEMENTED();
  return false;
}

bool StubInputController::HasTouchpad() {
  NOTIMPLEMENTED();
  return false;
}

void StubInputController::SetTouchpadSensitivity(int value) {
  NOTIMPLEMENTED();
}

void StubInputController::SetTapToClick(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetThreeFingerClick(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetTapDragging(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetNaturalScroll(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetMouseSensitivity(int value) {
  NOTIMPLEMENTED();
}

void StubInputController::SetPrimaryButtonRight(bool right) {
  NOTIMPLEMENTED();
}

}  // namespace

scoped_ptr<InputController> CreateStubInputController() {
  return make_scoped_ptr(new StubInputController);
}

}  // namespace ui
