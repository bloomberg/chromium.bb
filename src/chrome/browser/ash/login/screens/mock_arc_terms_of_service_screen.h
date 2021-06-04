// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_

#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockArcTermsOfServiceScreen : public ArcTermsOfServiceScreen {
 public:
  MockArcTermsOfServiceScreen(ArcTermsOfServiceScreenView* view,
                              const ScreenExitCallback& exit_callback);
  ~MockArcTermsOfServiceScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockArcTermsOfServiceScreenView : public ArcTermsOfServiceScreenView {
 public:
  MockArcTermsOfServiceScreenView();
  ~MockArcTermsOfServiceScreenView() override;

  void AddObserver(ArcTermsOfServiceScreenViewObserver* observer) override;
  void RemoveObserver(ArcTermsOfServiceScreenViewObserver* observer) override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, Bind, (ArcTermsOfServiceScreen * screen));
  MOCK_METHOD(void,
              MockAddObserver,
              (ArcTermsOfServiceScreenViewObserver * observer));
  MOCK_METHOD(void,
              MockRemoveObserver,
              (ArcTermsOfServiceScreenViewObserver * observer));

 private:
  ArcTermsOfServiceScreenViewObserver* observer_ = nullptr;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockArcTermsOfServiceScreen;
using ::ash::MockArcTermsOfServiceScreenView;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_
