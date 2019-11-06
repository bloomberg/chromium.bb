// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SHELF_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SHELF_TEST_HELPER_H_
#include "ash/public/interfaces/login_screen_test_api.test-mojom.h"

namespace chromeos {

class LoginShelfTestHelper {
 public:
  LoginShelfTestHelper();
  ~LoginShelfTestHelper();

  // Returns true if the login shelf is visible.
  bool IsLoginShelfShown();

 private:
  ash::mojom::LoginScreenTestApiPtr login_screen_;

  DISALLOW_COPY_AND_ASSIGN(LoginShelfTestHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_SHELF_TEST_HELPER_H_
