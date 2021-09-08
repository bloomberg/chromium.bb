// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAutoEnrollmentCheckScreen : public AutoEnrollmentCheckScreen {
 public:
  MockAutoEnrollmentCheckScreen(AutoEnrollmentCheckScreenView* view,
                                ErrorScreen* error_screen,
                                const base::RepeatingClosure& exit_callback);
  ~MockAutoEnrollmentCheckScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void RealShow();
  void ExitScreen();
};

class MockAutoEnrollmentCheckScreenView : public AutoEnrollmentCheckScreenView {
 public:
  MockAutoEnrollmentCheckScreenView();
  ~MockAutoEnrollmentCheckScreenView() override;

  void SetDelegate(Delegate* screen) override;

  MOCK_METHOD(void, MockSetDelegate, (Delegate * screen));
  MOCK_METHOD(void, Show, ());

 private:
  Delegate* screen_ = nullptr;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockAutoEnrollmentCheckScreen;
using ::ash::MockAutoEnrollmentCheckScreenView;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
