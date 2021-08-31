// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"

namespace base {
class RunLoop;
}

namespace chromeos {

// A waiter that blocks until the the current WizardController screen is
// different than the target screen, or the WizardController is destroyed.
class WizardControllerExitWaiter : public test::TestConditionWaiter,
                                   public WizardController::ScreenObserver {
 public:
  explicit WizardControllerExitWaiter(OobeScreenId screen_id);
  explicit WizardControllerExitWaiter(BaseScreen* target_screen);
  ~WizardControllerExitWaiter() override;

  // WizardController::ScreenObserver:
  void OnCurrentScreenChanged(BaseScreen* new_screen) override;
  void OnShutdown() override;

  // TestConditionWaiter;
  void Wait() override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN_EXIT, DONE };

  void EndWait();

  const BaseScreen* target_screen_;

  State state_ = State::IDLE;

  base::ScopedObservation<WizardController, WizardController::ScreenObserver>
      screen_observation_{this};

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::WizardControllerExitWaiter;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_WIZARD_CONTROLLER_SCREEN_EXIT_WAITER_H_
