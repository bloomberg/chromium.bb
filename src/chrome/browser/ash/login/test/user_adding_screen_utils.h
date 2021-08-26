// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_USER_ADDING_SCREEN_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_USER_ADDING_SCREEN_UTILS_H_

namespace chromeos {
namespace test {

// Utility function for user adding screen that waits for the user adding screen
// to be shown.
void ShowUserAddingScreen();

}  // namespace test
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace test {
using ::chromeos::test::ShowUserAddingScreen;
}
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_USER_ADDING_SCREEN_UTILS_H_
