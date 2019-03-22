// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_test_api.h"

#include <memory>
#include <utility>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_password_view.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// static
void LoginScreenTestApi::BindRequest(mojom::LoginScreenTestApiRequest request) {
  mojo::MakeStrongBinding(std::make_unique<LoginScreenTestApi>(),
                          std::move(request));
}

LoginScreenTestApi::LoginScreenTestApi() = default;

LoginScreenTestApi::~LoginScreenTestApi() = default;

void LoginScreenTestApi::IsLockShown(IsLockShownCallback callback) {
  std::move(callback).Run(
      LockScreen::HasInstance() && LockScreen::Get()->is_shown() &&
      LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLock);
}

void LoginScreenTestApi::SubmitPassword(const AccountId& account_id,
                                        const std::string& password,
                                        SubmitPasswordCallback callback) {
  // It'd be better to generate keyevents dynamically and dispatch them instead
  // of reaching into the views structure, but at the time of writing I could
  // not find a good way to do this. If you know of a way feel free to change
  // this code.
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  LoginAuthUserView::TestApi auth_test(
      lock_contents_test.primary_big_view()->auth_user());
  LoginPasswordView::TestApi password_test(auth_test.password_view());

  // For the time being, only the primary user is supported. To support multiple
  // users this API needs to search all user views for the associated user and
  // potentially activate that user so it is showing its password field.
  CHECK_EQ(account_id,
           auth_test.user_view()->current_user()->basic_user_info->account_id);

  password_test.SubmitPassword(password);

  std::move(callback).Run();
}

}  // namespace ash
