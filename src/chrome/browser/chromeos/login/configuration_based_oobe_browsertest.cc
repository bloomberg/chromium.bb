// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/hid_controller_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ime/chromeos/input_method_util.h"

// Disabled due to flakiness: https://crbug.com/997685.
#define MAYBE_TestDemoModeOfflineNetwork DISABLED_TestDemoModeOfflineNetwork
#define MAYBE_TestDemoModeAcceptEula DISABLED_TestDemoModeAcceptEula

namespace chromeos {

// This test case will use
// src/chromeos/test/data/oobe_configuration/<TestName>.json file as
// OOBE configuration for each of the tests and verify that relevant parts
// of OOBE automation took place. OOBE WebUI will not be started until
// LoadConfiguration() is called to allow configure relevant stubs.
class OobeConfigurationTest : public OobeBaseTest {
 public:
  OobeConfigurationTest() = default;

  bool ShouldWaitForOobeUI() override { return false; }

  void LoadConfiguration() {
    OobeConfiguration::set_skip_check_for_testing(false);
    // Make sure configuration is loaded
    base::RunLoop run_loop;
    OOBEConfigurationWaiter waiter;

    OobeConfiguration::Get()->CheckConfiguration();

    const bool ready = waiter.IsConfigurationLoaded(run_loop.QuitClosure());
    if (!ready)
      run_loop.Run();

    // Let screens to settle.
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOfflineEnvironment() {
    DemoSetupController* controller =
        WizardController::default_controller()->demo_setup_controller();

    // Simulate offline data directory.
    ASSERT_TRUE(
        chromeos::test::SetupDummyOfflinePolicyDir("test", &fake_policy_dir_));
    controller->SetPreinstalledOfflineResourcesPathForTesting(
        fake_policy_dir_.GetPath());
  }


  void SetUpCommandLine(base::CommandLine* command_line) override {
    // File name is based on the test name.
    base::FilePath file;
    ASSERT_TRUE(GetTestFileName(".json", &file));

    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   file);

    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  // Stores a name of the configuration data file to |file|.
  // Returns true iff |file| exists.
  bool GetTestFileName(const std::string& suffix, base::FilePath* file) {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string filename = std::string(test_info->name()) + suffix;
    return test_utils::GetTestDataPath("oobe_configuration", filename, file);
  }

  // Overridden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Set up fake networks.
    // TODO(pmarko): Find a way for FakeShillManagerClient to be initialized
    // automatically (https://crbug.com/847422).
    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();

    OobeBaseTest::SetUpOnMainThread();
    LoadConfiguration();

    // Make sure that OOBE is run as an "official" build.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");
  }

 protected:
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
  base::ScopedTempDir fake_policy_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationTest);
};

// EnterpriseEnrollmentConfigurationTest with no input devices.
class OobeConfigurationTestNoHID : public OobeConfigurationTest {
 public:
  OobeConfigurationTestNoHID() {
    // HID detection screen only appears for Chromebases, Chromebits, and
    // Chromeboxes.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now());
  }

  ~OobeConfigurationTestNoHID() override = default;

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationTestNoHID);
};

class OobeConfigurationEnrollmentTest : public OobeConfigurationTest {
 public:
  OobeConfigurationEnrollmentTest() = default;
  ~OobeConfigurationEnrollmentTest() override = default;

 protected:
  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  // We need fake gaia to fetch device local account tokens.
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationEnrollmentTest);
};

// Check that configuration lets correctly pass Welcome screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestLeaveWelcomeScreen) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Check that language and input methods are set correctly.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSwitchLanguageIME) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();

  // Configuration specified in TestSwitchLanguageIME.json sets non-default
  // input method fo German (xkb:de:neo:ger) to ensure that input method value
  // is propagated correctly. We need to migrate public IME name to internal
  // scheme to be able to compare them.

  const std::string ime_id =
      imm->GetInputMethodUtil()->MigrateInputMethod("xkb:de:neo:ger");
  EXPECT_EQ(ime_id, imm->GetActiveIMEState()->GetCurrentInputMethod().id());

  const std::string language_code = g_browser_process->local_state()->GetString(
      language::prefs::kApplicationLocale);
  EXPECT_EQ("de", language_code);
}

// Check that configuration lets correctly start Demo mode setup.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestEnableDemoMode) {
  LoadConfiguration();
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
}

// Check that configuration lets correctly pass through demo preferences.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestDemoModePreferences) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Check that configuration lets correctly use offline demo mode on network
// screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest,
                       MAYBE_TestDemoModeOfflineNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  SimulateOfflineEnvironment();
  OobeScreenWaiter(EulaView::kScreenId).Wait();
}

// Check that configuration lets correctly use offline demo mode on EULA
// screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, MAYBE_TestDemoModeAcceptEula) {
  LoadConfiguration();
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  SimulateOfflineEnvironment();
  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
}

// Check that configuration lets correctly use offline demo mode on ARC++ ToS
// screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestDemoModeAcceptArcTos) {
  LoadConfiguration();
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  SimulateOfflineEnvironment();

  test::OobeJS().Evaluate(
      "login.ArcTermsOfServiceScreen.setTosForTesting('Test "
      "Play Store Terms of Service');");
  test::OobeJS().Evaluate(
      "$('demo-preferences-content').$$('oobe-dialog')."
      "querySelector('oobe-text-button').click();");

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
}

// Check that configuration lets correctly select a network by GUID.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSelectNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(EulaView::kScreenId).Wait();
}

// Check that configuration would proceed if there is a connected network.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestSelectConnectedNetwork) {
  LoadConfiguration();
  OobeScreenWaiter(EulaView::kScreenId).Wait();
}

// Check that configuration would not proceed with connected network if
// welcome screen is not automated.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestConnectedNetworkNoWelcome) {
  LoadConfiguration();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

// Check that when configuration has ONC and EULA, we get to update screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestAcceptEula) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_progress(0.1);
  update_engine_client()->set_default_status(status);

  LoadConfiguration();
  OobeScreenWaiter(UpdateView::kScreenId).Wait();
}

// Check that when configuration has requisition, it gets applied at the
// beginning.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTest, TestDeviceRequisition) {
  LoadConfiguration();
  OobeScreenWaiter(EulaView::kScreenId).Wait();
  auto* policy_manager = g_browser_process->platform_part()
                             ->browser_policy_connector_chromeos()
                             ->GetDeviceCloudPolicyManager();
  EXPECT_EQ(policy_manager->GetDeviceRequisition(), "some_requisition");
}

// Check that configuration allows to skip Update screen and get to Enrollment
// screen.
IN_PROC_BROWSER_TEST_F(OobeConfigurationEnrollmentTest, TestSkipUpdate) {
  LoadConfiguration();
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);
}

IN_PROC_BROWSER_TEST_F(OobeConfigurationEnrollmentTest, TestEnrollUsingToken) {
  policy_server_.SetUpdateDeviceAttributesPermission(false);
  policy_server_.SetFakeAttestationFlow();

  // Token from configuration file:
  policy_server_.ExpectTokenEnrollment("00000000-1111-2222-3333-444444444444",
                                       FakeGaiaMixin::kEnterpriseUser1);
  LoadConfiguration();
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

// Check that HID detection screen is shown if it is not specified by
// configuration.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTestNoHID, TestShowHID) {
  LoadConfiguration();
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
}

// Check that HID detection screen is really skipped and rest of configuration
// is applied.
IN_PROC_BROWSER_TEST_F(OobeConfigurationTestNoHID, TestSkipHIDDetection) {
  LoadConfiguration();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

}  // namespace chromeos
