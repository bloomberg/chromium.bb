// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_

#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_launcher.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockWebKioskAppLauncher : public WebKioskAppLauncher {
 public:
  MockWebKioskAppLauncher();
  ~MockWebKioskAppLauncher() override;

  MOCK_METHOD1(Initialize, void(const AccountId&));
  MOCK_METHOD0(ContinueWithNetworkReady, void());
  MOCK_METHOD0(LaunchApp, void());
  MOCK_METHOD0(CancelCurrentInstallation, void());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_MOCK_WEB_KIOSK_APP_LAUNCHER_H_
