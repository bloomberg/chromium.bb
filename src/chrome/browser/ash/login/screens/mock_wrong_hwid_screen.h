// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_

#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockWrongHWIDScreen : public WrongHWIDScreen {
 public:
  MockWrongHWIDScreen(WrongHWIDScreenView* view,
                      const base::RepeatingClosure& exit_callback);
  ~MockWrongHWIDScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen();
};

class MockWrongHWIDScreenView : public WrongHWIDScreenView {
 public:
  MockWrongHWIDScreenView();
  ~MockWrongHWIDScreenView() override;

  void Bind(WrongHWIDScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, MockBind, (WrongHWIDScreen*));
  MOCK_METHOD(void, MockUnbind, ());

 private:
  WrongHWIDScreen* screen_ = nullptr;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockWrongHWIDScreen;
using ::ash::MockWrongHWIDScreenView;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WRONG_HWID_SCREEN_H_
