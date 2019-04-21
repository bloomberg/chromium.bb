// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_TEST_UTIL_H_
#define ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_TEST_UTIL_H_

namespace ash {

class TestSessionControllerClient;

// Logs in a user with Kiosk Next enabled.
void LogInKioskNextUser(TestSessionControllerClient* session_controller_client);

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_TEST_UTIL_H_
