// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"

namespace content {
class WebUI;
class WindowedNotificationObserver;
}  // namespace content

namespace chromeos {

// Base class for OOBE, login, SAML and Kiosk tests.
class OobeBaseTest : public MixinBasedInProcessBrowserTest {
 public:
  OobeBaseTest();
  ~OobeBaseTest() override;

  // Subclasses may register their own custom request handlers that will
  // process requests prior it gets handled by FakeGaia instance.
  virtual void RegisterAdditionalRequestHandlers();

 protected:
  // MixinBasedInProcessBrowserTest::
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // If this returns true (default), the |ash::switches::kShowWebUiLogin|
  // command-line switch is passed to force the Web Ui Login.
  // If this returns false, the switch is omitted so the views-based login may
  // be used.
  virtual bool ShouldForceWebUiLogin();

  // If this returns true (default), then SetUpOnMainThread would wait for
  // Oobe UI to start up before initializing all mix-ins.
  virtual bool ShouldWaitForOobeUI();

  // Returns chrome://oobe WebUI.
  content::WebUI* GetLoginUI();

  void WaitForOobeUI();
  void WaitForGaiaPageLoad();
  void WaitForGaiaPageLoadAndPropertyUpdate();
  void WaitForGaiaPageReload();
  void WaitForGaiaPageBackButtonUpdate();
  void WaitForGaiaPageEvent(const std::string& event);
  void WaitForSigninScreen();
  void WaitForEnrollmentSuccess();
  test::JSChecker SigninFrameJS();

  // Whether to use background networking. Note this is only effective when it
  // is set before SetUpCommandLine is invoked.
  bool needs_background_networking_ = false;

  std::unique_ptr<content::WindowedNotificationObserver>
      login_screen_load_observer_;
  std::string gaia_frame_parent_ = "signin-frame";
  std::string authenticator_id_ = "$('gaia-signin').gaiaAuthHost_";

  DISALLOW_COPY_AND_ASSIGN(OobeBaseTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_BASE_TEST_H_
