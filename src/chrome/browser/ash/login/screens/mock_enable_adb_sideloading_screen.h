// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_

#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEnableAdbSideloadingScreen : public EnableAdbSideloadingScreen {
 public:
  MockEnableAdbSideloadingScreen(EnableAdbSideloadingScreenView* view,
                                 const base::RepeatingClosure& exit_callback);
  ~MockEnableAdbSideloadingScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen();
};

class MockEnableAdbSideloadingScreenView
    : public EnableAdbSideloadingScreenView {
 public:
  MockEnableAdbSideloadingScreenView();
  ~MockEnableAdbSideloadingScreenView() override;

  void Bind(EnableAdbSideloadingScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, MockBind, (EnableAdbSideloadingScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());
  MOCK_METHOD(void, SetScreenState, (UIState value));

 private:
  EnableAdbSideloadingScreen* screen_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockEnableAdbSideloadingScreen;
using ::ash::MockEnableAdbSideloadingScreenView;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ENABLE_ADB_SIDELOADING_SCREEN_H_
