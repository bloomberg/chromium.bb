// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_PROFILE_PREPARED_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_PROFILE_PREPARED_WAITER_H_

#include "ash/components/login/auth/auth_status_consumer.h"
#include "base/run_loop.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace test {

// Wait for ExistingUserController::OnProfilePrepared
class ProfilePreparedWaiter : public AuthStatusConsumer {
 public:
  explicit ProfilePreparedWaiter(const AccountId& account_id);
  ProfilePreparedWaiter(const ProfilePreparedWaiter&) = delete;
  ProfilePreparedWaiter& operator=(const ProfilePreparedWaiter&) = delete;

  ~ProfilePreparedWaiter() override;

  // AuthStatusConsumer
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnAuthFailure(const AuthFailure& error) override;

  void Wait();

 private:
  const AccountId account_id_;
  bool done_ = false;

  base::RunLoop run_loop_;
};

}  // namespace test
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace test {
using ::ash::test::ProfilePreparedWaiter;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_PROFILE_PREPARED_WAITER_H_
