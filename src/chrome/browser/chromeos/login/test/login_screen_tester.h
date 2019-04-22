// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_SCREEN_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_SCREEN_TESTER_H_

#include <cstdint>

#include "ash/public/interfaces/login_screen_test_api.test-mojom-test-utils.h"
#include "base/macros.h"

namespace chromeos {
namespace test {

// High-level API to ash::mojom::LoginScreenTestApi.
class LoginScreenTester {
 public:
  LoginScreenTester();
  ~LoginScreenTester();

  // Blocking mojo calls.
  int64_t GetUiUpdateCount();
  bool IsRestartButtonShown();
  bool IsShutdownButtonShown();

  // Returns true on success (i.e. button is  not disabled).
  bool ClickAddUserButton();

  // Blocks until LoginShelfView::ui_update_count() is greater then
  // |previous_update_count|. Returns true on success, false on error.
  bool WaitForUiUpdate(int64_t previous_update_count);

 private:
  ash::mojom::LoginScreenTestApiPtr test_api_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenTester);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_SCREEN_TESTER_H_
