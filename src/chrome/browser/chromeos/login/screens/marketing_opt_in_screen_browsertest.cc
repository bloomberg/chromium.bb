// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include <initializer_list>
#include <memory>
#include <string>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/marketing_backend_connector.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/c/system/trap.h"

namespace chromeos {

namespace {

const std::initializer_list<base::StringPiece> kChromebookEmailToggle = {
    "marketing-opt-in", "chromebookUpdatesOption"};
const std::initializer_list<base::StringPiece> kChromebookEmailToggleDiv = {
    "marketing-opt-in", "marketing-opt-in-toggle"};
const std::initializer_list<base::StringPiece> kMarketingA11yButton = {
    "marketing-opt-in", "marketing-opt-in-accessibility-button"};
const std::initializer_list<base::StringPiece> kMarketingFinalA11yPage = {
    "marketing-opt-in", "finalAccessibilityPage"};
const std::initializer_list<base::StringPiece> kMarketingA11yButtonToggle = {
    "marketing-opt-in", "a11yNavButtonToggle"};

// Parameter to be used in tests.
struct RegionToCodeMap {
  const char* test_name;
  const char* region;
  const char* country_code;
  bool is_default_opt_in;
  bool is_unknown_country;
};

// Default countries
const RegionToCodeMap kDefaultCountries[]{
    {"US", "America/Los_Angeles", "us", true, false},
    {"Canada", "Canada/Atlantic", "ca", false, false},
    {"UnitedKingdom", "Europe/London", "gb", false, false}};

// Extended region list. Behind feature flag.
const RegionToCodeMap kExtendedCountries[]{
    {"France", "Europe/Paris", "fr", false, false},
    {"Netherlands", "Europe/Amsterdam", "nl", false, false},
    {"Finland", "Europe/Helsinki", "fi", false, false},
    {"Sweden", "Europe/Stockholm", "se", false, false},
    {"Norway", "Europe/Oslo", "no", false, false},
    {"Denmark", "Europe/Copenhagen", "dk", false, false},
    {"Spain", "Europe/Madrid", "es", false, false},
    {"Italy", "Europe/Rome", "it", false, false},
    {"Japan", "Asia/Tokyo", "jp", false, false},
    {"Australia", "Australia/Sydney", "au", false, false}};

// Double opt-in countries. Behind double opt-in feature flag.
const RegionToCodeMap kDoubleOptInCountries[]{
    {"Germany", "Europe/Berlin", "de", false, false}};

// Unknown country.
const RegionToCodeMap kUnknownCountry[]{
    {"Unknown", "unknown", "", false, true}};
}  // namespace

// Base class for simple tests on the marketing opt-in screen.
class MarketingOptInScreenTest : public OobeBaseTest,
                                 public LocalStateMixin::Delegate {
 public:
  MarketingOptInScreenTest();
  ~MarketingOptInScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override;

  MarketingOptInScreen* GetScreen();
  void ShowMarketingOptInScreen();
  void TapOnGetStartedAndWaitForScreenExit();
  void ShowAccessibilityButtonForTest();
  // Expects that the option to opt-in is not visible.
  void ExpectNoOptInOption();
  // Expects that the option to opt-in is visible.
  void ExpectOptInOptionAvailable();
  // Expects that the opt-in toggle is visible and unchecked.
  void ExpectOptedOut();
  // Expects that the opt-in toggle is visible and checked.
  void ExpectOptedIn();
  // Flips the toggle to opt-in. Only to be called when the toggle is unchecked.
  void OptIn();

  void ExpectGeolocationMetric(bool resolved, int length);
  void WaitForScreenExit();
  void SetUpLocalState() override {}

  base::Optional<MarketingOptInScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 protected:
  base::test::ScopedFeatureList feature_list_;
 private:
  void HandleScreenExit(MarketingOptInScreen::Result result);

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  MarketingOptInScreen::ScreenExitCallback original_callback_;

  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

/**
 * For testing backend requests
 */
class MarketingOptInScreenTestWithRequest : public MarketingOptInScreenTest {
 public:
  MarketingOptInScreenTestWithRequest() = default;
  ~MarketingOptInScreenTestWithRequest() override = default;

  void WaitForBackendRequest();
  void HandleBackendRequest(std::string country_code);
  std::string GetRequestedCountryCode() { return requested_country_code_; }

 private:
  bool backend_request_performed_ = false;
  base::RepeatingClosure backend_request_callback_;
  std::string requested_country_code_;

  ScopedRequestCallbackSetter callback_setter{
      std::make_unique<base::RepeatingCallback<void(std::string)>>(
          base::BindRepeating(
              &MarketingOptInScreenTestWithRequest::HandleBackendRequest,
              base::Unretained(this)))};
};

MarketingOptInScreenTest::MarketingOptInScreenTest() {
  // To reuse existing wizard controller in the flow.
  feature_list_.InitWithFeatures(
      {chromeos::features::kOobeScreensPriority,
       ::features::kOobeMarketingDoubleOptInCountriesSupported,
       ::features::kOobeMarketingAdditionalCountriesSupported},
      {});
}

void MarketingOptInScreenTest::SetUpOnMainThread() {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  original_callback_ = GetScreen()->get_exit_callback_for_testing();
  GetScreen()->set_exit_callback_for_testing(base::BindRepeating(
      &MarketingOptInScreenTest::HandleScreenExit, base::Unretained(this)));

  OobeBaseTest::SetUpOnMainThread();
  login_manager_mixin_.LoginAsNewReguarUser();
  OobeScreenExitWaiter(GaiaView::kScreenId).Wait();
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kGestureEducationNotificationShown, true);
}

MarketingOptInScreen* MarketingOptInScreenTest::GetScreen() {
  return MarketingOptInScreen::Get(
      WizardController::default_controller()->screen_manager());
}

void MarketingOptInScreenTest::ShowMarketingOptInScreen() {
  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);
}

void MarketingOptInScreenTest::TapOnGetStartedAndWaitForScreenExit() {
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "marketing-opt-in-next-button"});
  WaitForScreenExit();
}

void MarketingOptInScreenTest::ShowAccessibilityButtonForTest() {
  GetScreen()->SetA11yButtonVisibilityForTest(true /* shown */);
}

void MarketingOptInScreenTest::ExpectNoOptInOption() {
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  test::OobeJS().ExpectHiddenPath(kChromebookEmailToggleDiv);
}

void MarketingOptInScreenTest::ExpectOptInOptionAvailable() {
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  test::OobeJS().ExpectVisiblePath(kChromebookEmailToggleDiv);
}

void MarketingOptInScreenTest::ExpectOptedOut() {
  ExpectOptInOptionAvailable();
  test::OobeJS().ExpectHasNoAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::ExpectOptedIn() {
  ExpectOptInOptionAvailable();
  test::OobeJS().ExpectHasAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::OptIn() {
  test::OobeJS().ClickOnPath(kChromebookEmailToggle);
  test::OobeJS().ExpectHasAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::ExpectGeolocationMetric(bool resolved,
                                                       int length) {
  histogram_tester_.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.GeolocationResolve",
      resolved
          ? MarketingOptInScreen::GeolocationEvent::
                kCountrySuccessfullyDetermined
          : MarketingOptInScreen::GeolocationEvent::kCouldNotDetermineCountry,
      1);
  if (resolved) {
    histogram_tester_.ExpectUniqueSample(
        "OOBE.MarketingOptInScreen.GeolocationResolveLength", length, 1);
  }
}

void MarketingOptInScreenTest::WaitForScreenExit() {
  if (screen_exited_)
    return;

  base::RunLoop run_loop;
  screen_exit_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MarketingOptInScreenTest::HandleScreenExit(
    MarketingOptInScreen::Result result) {
  ASSERT_FALSE(screen_exited_);
  screen_exited_ = true;
  screen_result_ = result;
  original_callback_.Run(result);
  if (screen_exit_callback_)
    std::move(screen_exit_callback_).Run();
}

void MarketingOptInScreenTestWithRequest::WaitForBackendRequest() {
  if (backend_request_performed_)
    return;
  base::RunLoop run_loop;
  backend_request_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MarketingOptInScreenTestWithRequest::HandleBackendRequest(
    std::string country_code) {
  ASSERT_FALSE(backend_request_performed_);
  backend_request_performed_ = true;
  requested_country_code_ = country_code;
  if (backend_request_callback_)
    std::move(backend_request_callback_).Run();
}

// Tests that the screen is visible
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ScreenVisible) {
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
}

// Tests that the user can enable shelf navigation buttons in tablet mode from
// the screen.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, EnableShelfNavigationButtons) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingA11yButton)->Wait();
  test::OobeJS().ClickOnPath(kMarketingA11yButton);
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingFinalA11yPage)->Wait();

  // Tap the shelf navigation buttons in tablet mode toggle.
  test::OobeJS()
      .CreateVisibilityWaiter(true, kMarketingA11yButtonToggle)
      ->Wait();
  test::OobeJS().ClickOnPath(
      {"marketing-opt-in", "a11yNavButtonToggle", "button"});

  // Go back to the first screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-back-button"});

  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketingOptInOverviewDialog"})
      ->Wait();

  TapOnGetStartedAndWaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MarketingOptInScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     1);

  // Verify the accessibility pref for shelf navigation buttons is set.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

// Tests that the user can exit the screen from the accessibility page.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ExitScreenFromA11yPage) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingA11yButton)->Wait();
  test::OobeJS().ClickOnPath(kMarketingA11yButton);
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingFinalA11yPage)->Wait();

  // Tapping the next button exits the screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-next-button"});
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MarketingOptInScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     1);
}

// Interface for setting up parameterized tests based on the region.
class RegionAsParameterInterface
    : public ::testing::WithParamInterface<RegionToCodeMap> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<RegionToCodeMap> param_info) {
    return param_info.param.test_name;
  }

  void SetUpLocalStateRegion() {
    RegionToCodeMap param = GetParam();
    g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                                param.region);
  }
};

// Tests that all country codes are correct given the timezone.
class MarketingTestCountryCodes : public MarketingOptInScreenTestWithRequest,
                                  public RegionAsParameterInterface {
 public:
  MarketingTestCountryCodes() = default;
  ~MarketingTestCountryCodes() = default;

  void SetUpLocalState() override { SetUpLocalStateRegion(); }
};

// Tests that the given timezone resolves to the correct location and
// generates a request for the server with the correct region code.
IN_PROC_BROWSER_TEST_P(MarketingTestCountryCodes, CountryCodes) {
  const RegionToCodeMap param = GetParam();
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  if (param.is_default_opt_in) {
    ExpectOptedIn();
  } else {
    ExpectOptedOut();
    OptIn();
  }

  TapOnGetStartedAndWaitForScreenExit();
  WaitForBackendRequest();
  EXPECT_EQ(GetRequestedCountryCode(), param.country_code);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.Event." + std::string(param.country_code),
      (param.is_default_opt_in)
          ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptIn
          : MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptOut,
      1);

  // Expect successful geolocation resolve.
  ExpectGeolocationMetric(true, std::string(param.country_code).size());
}

// Test all the countries lists.
INSTANTIATE_TEST_SUITE_P(MarketingOptInDefaultCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kDefaultCountries),
                         RegionAsParameterInterface::ParamInfoToString);
INSTANTIATE_TEST_SUITE_P(MarketingOptInExtendedCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kExtendedCountries),
                         RegionAsParameterInterface::ParamInfoToString);
INSTANTIATE_TEST_SUITE_P(MarketingOptInDoubleOptInCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kDoubleOptInCountries),
                         RegionAsParameterInterface::ParamInfoToString);

// Disables the extended list of countries and the double opt-in countries.
class MarketingDisabledExtraCountries : public MarketingOptInScreenTest,
                                        public RegionAsParameterInterface {
 public:
  MarketingDisabledExtraCountries() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {chromeos::features::kOobeScreensPriority},
        {::features::kOobeMarketingDoubleOptInCountriesSupported,
         ::features::kOobeMarketingAdditionalCountriesSupported});
  }

  ~MarketingDisabledExtraCountries() = default;

  void SetUpLocalState() override { SetUpLocalStateRegion(); }
};

IN_PROC_BROWSER_TEST_P(MarketingDisabledExtraCountries, OptInNotVisible) {
  const RegionToCodeMap param = GetParam();
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  ExpectNoOptInOption();
  TapOnGetStartedAndWaitForScreenExit();

  if (param.is_unknown_country)
    ExpectGeolocationMetric(false, 0);
  else
    ExpectGeolocationMetric(true, std::string(param.country_code).size());
}

// Tests that countries from the extended list
// cannot opt-in when the feature is disabled.
INSTANTIATE_TEST_SUITE_P(MarketingOptInExtendedCountries,
                         MarketingDisabledExtraCountries,
                         testing::ValuesIn(kExtendedCountries),
                         RegionAsParameterInterface::ParamInfoToString);

// Tests that double opt-in countries cannot opt-in
// when the feature is disabled.
INSTANTIATE_TEST_SUITE_P(MarketingOptInDoubleOptInCountries,
                         MarketingDisabledExtraCountries,
                         testing::ValuesIn(kDoubleOptInCountries),
                         RegionAsParameterInterface::ParamInfoToString);

// Tests that unknown countries cannot opt-in.
INSTANTIATE_TEST_SUITE_P(MarketingOptInUnknownCountries,
                         MarketingDisabledExtraCountries,
                         testing::ValuesIn(kUnknownCountry),
                         RegionAsParameterInterface::ParamInfoToString);

class MarketingOptInScreenTestDisabled : public MarketingOptInScreenTest {
 public:
  MarketingOptInScreenTestDisabled() {
    feature_list_.Reset();
    // Enable |kOobeScreensPriority| to reuse existing wizard controller in
    // the flow and disable kOobeMarketingScreen to disable marketing screen.
    feature_list_.InitWithFeatures({chromeos::features::kOobeScreensPriority},
                                   {::features::kOobeMarketingScreen});
  }

  ~MarketingOptInScreenTestDisabled() override = default;
};

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTestDisabled, FeatureDisabled) {
  ShowMarketingOptInScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            MarketingOptInScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     0);
}

}  // namespace chromeos
