// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/user_adding_screen_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/login_screen_shown_observer.h"

namespace chromeos {
namespace test {

namespace {

// A waiter that blocks until the target login screen is reached.
class LoginScreenWaiter : public LoginScreenShownObserver {
 public:
  LoginScreenWaiter() {
    LoginScreenClient::Get()->AddLoginScreenShownObserver(this);
  }
  ~LoginScreenWaiter() override {
    LoginScreenClient::Get()->RemoveLoginScreenShownObserver(this);
  }
  LoginScreenWaiter(const LoginScreenWaiter&) = delete;
  LoginScreenWaiter& operator=(const LoginScreenWaiter&) = delete;

  // LoginScreenShownObserver:
  void OnLoginScreenShown() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

void ShowUserAddingScreen() {
  LoginScreenWaiter waiter;
  UserAddingScreen::Get()->Start();
  waiter.Wait();
}

}  // namespace test
}  // namespace chromeos
