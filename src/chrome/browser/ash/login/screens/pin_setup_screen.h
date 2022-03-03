// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_

#include <string>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/wizard_context.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class PinSetupScreen : public BaseScreen {
 public:
  using TView = PinSetupScreenView;
  enum class Result { DONE, USER_SKIP, NOT_APPLICABLE, TIMED_OUT };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class UserAction {
    kDoneButtonClicked = 0,
    kSkipButtonClickedOnStart = 1,
    kSkipButtonClickedInFlow = 2,
    kMaxValue = kSkipButtonClickedInFlow
  };

  static std::string GetResultString(Result result);

  // Checks whether PIN setup should be skipped because of the policies.
  // There is an additional checkpoint that might skip the setup based on user
  // profile and pin availability information in `MaybeSkip`.
  static bool ShouldSkipBecauseOfPolicy();

  static std::unique_ptr<base::AutoReset<bool>>
  SetForceNoSkipBecauseOfPolicyForTests(bool value);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  PinSetupScreen(PinSetupScreenView* view,
                 const ScreenExitCallback& exit_callback);

  PinSetupScreen(const PinSetupScreen&) = delete;
  PinSetupScreen& operator=(const PinSetupScreen&) = delete;

  ~PinSetupScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  // Inticates whether the device supports usage of PIN for login.
  // This information is retrived in an async way and will not be available
  // immediately.
  absl::optional<bool> has_login_support_;

  PinSetupScreenView* const view_;
  ScreenExitCallback exit_callback_;

  base::OneShotTimer token_lifetime_timeout_;

  bool SkipScreen(WizardContext* context);
  void ClearAuthData(WizardContext* context);
  void OnHasLoginSupport(bool login_available);
  void OnTokenTimedOut();

  base::WeakPtrFactory<PinSetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::PinSetupScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
