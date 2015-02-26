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
  ~StubInputController() override;

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
  void GetTouchEventLog(const base::FilePath& out_dir,
                        const GetTouchEventLogReply& reply) override;
  void DisableInternalTouchpad() override;
  void EnableInternalTouchpad() override;
  void DisableInternalKeyboardExceptKeys(
      scoped_ptr<std::set<DomCode>> excepted_keys) override;
  void EnableInternalKeyboard() override;

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

bool StubInputController::IsCapsLockEnabled() {
  return false;
}

void StubInputController::SetCapsLockEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetNumLockEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

bool StubInputController::IsAutoRepeatEnabled() {
  return true;
}

void StubInputController::SetAutoRepeatEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetAutoRepeatRate(const base::TimeDelta& delay,
                                            const base::TimeDelta& interval) {
  NOTIMPLEMENTED();
}

void StubInputController::GetAutoRepeatRate(base::TimeDelta* delay,
                                            base::TimeDelta* interval) {
  NOTIMPLEMENTED();
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

void StubInputController::SetTapToClickPaused(bool state) {
  NOTIMPLEMENTED();
}

void StubInputController::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  reply.Run(scoped_ptr<std::string>(new std::string));
}

void StubInputController::GetTouchEventLog(const base::FilePath& out_dir,
                                           const GetTouchEventLogReply& reply) {
  reply.Run(
      scoped_ptr<std::vector<base::FilePath>>(new std::vector<base::FilePath>));
}

void StubInputController::DisableInternalTouchpad() {
  NOTIMPLEMENTED();
}

void StubInputController::EnableInternalTouchpad() {
  NOTIMPLEMENTED();
}

void StubInputController::DisableInternalKeyboardExceptKeys(
    scoped_ptr<std::set<DomCode>> excepted_keys) {
  NOTIMPLEMENTED();
}

void StubInputController::EnableInternalKeyboard() {
  NOTIMPLEMENTED();
}

}  // namespace

scoped_ptr<InputController> CreateStubInputController() {
  return make_scoped_ptr(new StubInputController);
}

}  // namespace ui
