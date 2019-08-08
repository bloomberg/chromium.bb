// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace chromeos {

class EnrollmentScreenTest : public InProcessBrowserTest {
 public:
  EnrollmentScreenTest() = default;
  ~EnrollmentScreenTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(switches::kLoginManager);
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ShowLoginWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    EXPECT_EQ(WizardController::default_controller()->current_screen(),
              enrollment_screen());
    enrollment_screen()->set_exit_callback_for_testing(base::BindRepeating(
        &EnrollmentScreenTest::HandleScreenExit, base::Unretained(this)));
  }
  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_TRUE(WizardController::default_controller());
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_TRUE(enrollment_screen);
    return enrollment_screen;
  }

  // Runs loop until the enrollment screen reports exit. It will return the
  // last result returned by the enrollment screen.
  // NOTE: This will not work if the screen is expected to be shown more than
  // once - to support that use case, screen_result_ would have to be reset
  // before showing another EnrollmentScreen.
  EnrollmentScreen::Result WaitForScreenExit() {
    if (screen_result_.has_value())
      return screen_result_.value();
    if (!screen_exit_waiter_)
      screen_exit_waiter_ = std::make_unique<base::RunLoop>();
    screen_exit_waiter_->Run();
    return screen_result_.value();
  }

 private:
  void HandleScreenExit(EnrollmentScreen::Result result) {
    EXPECT_FALSE(screen_result_.has_value());
    screen_result_ = result;
    if (screen_exit_waiter_)
      screen_exit_waiter_->Quit();
  }

  base::Optional<EnrollmentScreen::Result> screen_result_;

  // Created lazily in WaitForScreenExit().
  // Note: unique_ptr because RunLoops cannot be created without a thread
  // context, which does not exist in the test ctor.
  std::unique_ptr<base::RunLoop> screen_exit_waiter_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestCancel) {
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestSuccess) {
  WizardController::SkipEnrollmentPromptsForTesting();
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

class AttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  AttestationAuthEnrollmentScreenTest() = default;
  ~AttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
  }

  DISALLOW_COPY_AND_ASSIGN(AttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(AttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, EnrollmentSpinner) {
  EnrollmentScreenView* view = enrollment_screen()->GetView();
  ASSERT_TRUE(view);

  test::JSChecker checker(
      LoginDisplayHost::default_host()->GetOobeWebContents());

  // Run through the flow
  view->Show();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "signin')).display !== 'none'");

  view->ShowEnrollmentSpinnerScreen();
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "working')).display !== 'none'");

  view->ShowAttestationBasedEnrollmentSuccessScreen("fake domain");
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "success')).display !== 'none'");
}

class ForcedAttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ForcedAttestationAuthEnrollmentScreenTest() = default;
  ~ForcedAttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "forced");
  }

  DISALLOW_COPY_AND_ASSIGN(ForcedAttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ForcedAttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

class MultiAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  MultiAuthEnrollmentScreenTest() = default;
  ~MultiAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
    // Kiosk mode will force OAuth enrollment.
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

  DISALLOW_COPY_AND_ASSIGN(MultiAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(MultiAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_TRUE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

class ProvisionedEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ProvisionedEnrollmentScreenTest() = default;
  ~ProvisionedEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

  DISALLOW_COPY_AND_ASSIGN(ProvisionedEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ProvisionedEnrollmentScreenTest, TestBackButton) {
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

}  // namespace chromeos
