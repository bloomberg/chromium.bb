// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"

namespace ash {

// Controller for the user creation screen.
class UserCreationScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum class Result {
    SIGNIN,
    CHILD_SIGNIN,
    CHILD_ACCOUNT_CREATE,
    ENTERPRISE_ENROLL,
    CANCEL,
    SKIPPED,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  explicit UserCreationScreen(UserCreationView* view,
                              ErrorScreen* error_screen,
                              const ScreenExitCallback& exit_callback);
  ~UserCreationScreen() override;

  UserCreationScreen(const UserCreationScreen&) = delete;
  UserCreationScreen& operator=(const UserCreationScreen&) = delete;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(UserCreationView* view);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  UserCreationView* view_ = nullptr;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  ErrorScreen* error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::UserCreationScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_CREATION_SCREEN_H_
