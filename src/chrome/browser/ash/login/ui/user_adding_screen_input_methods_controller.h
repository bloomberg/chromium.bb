// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace user_manager {
class User;
}

namespace ash {

// Ensure a set of "Login keyboard layouts" is used at UserAddingScreen.
class UserAddingScreenInputMethodsController
    : public UserAddingScreen::Observer {
 public:
  explicit UserAddingScreenInputMethodsController(UserAddingScreen* screen);

  UserAddingScreenInputMethodsController(
      const UserAddingScreenInputMethodsController&) = delete;
  UserAddingScreenInputMethodsController& operator=(
      const UserAddingScreenInputMethodsController&) = delete;

  ~UserAddingScreenInputMethodsController() override;

  // UserAddingScreen::Observer implementation:
  void OnBeforeUserAddingScreenStarted() override;
  void OnUserAddingFinished() override;

 private:
  UserAddingScreen* screen_;

  scoped_refptr<input_method::InputMethodManager::State> saved_ime_state_;
  user_manager::User* active_user_on_show_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_INPUT_METHODS_CONTROLLER_H_
