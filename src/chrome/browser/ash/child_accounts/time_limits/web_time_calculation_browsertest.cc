// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_activity_provider.h"

#include <vector>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kExampleHost1[] = "www.example.com";
constexpr char kExampleHost2[] = "www.example2.com";

}  // namespace

class WebTimeCalculationBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  WebTimeCalculationBrowserTest() = default;
  WebTimeCalculationBrowserTest(const WebTimeCalculationBrowserTest&) = delete;
  WebTimeCalculationBrowserTest& operator=(
      const WebTimeCalculationBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;

  void AllowlistUrlRegx(const std::string& url);
  Browser* DetachTabToNewBrowser(Browser* browser, int tab_index);
  content::WebContents* Navigate(Browser* browser,
                                 const std::string& url_in,
                                 WindowOpenDisposition disposition);

  app_time::ChromeAppActivityState GetChromeAppActivityState();

 private:
  void UpdatePolicy();

  Profile* profile_;

  base::test::ScopedFeatureList scoped_feature_list_;

  app_time::AppTimeLimitsAllowlistPolicyBuilder builder_;

  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_,
                                          LoggedInUserMixin::LogInType::kChild,
                                          embedded_test_server(), this};
};

void WebTimeCalculationBrowserTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {features::kWebTimeLimits},
      /* disabled_features */ {});

  builder_.SetUp();
  MixinBasedInProcessBrowserTest::SetUp();
}

void WebTimeCalculationBrowserTest::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->Started());

  logged_in_user_mixin_.LogInUser();
  profile_ = browser()->profile();

  // During tests, AppService doesn't notify AppActivityRegistry that chrome app
  // is installed. Mark chrome as installed here.
  auto* service = ChildUserServiceFactory::GetForBrowserContext(profile_);
  ChildUserService::TestApi test_api(service);
  test_api.app_time_controller()->app_registry()->OnAppInstalled(
      app_time::GetChromeAppId());
}

void WebTimeCalculationBrowserTest::TearDown() {
  builder_.Clear();
  MixinBasedInProcessBrowserTest::TearDown();
}

void WebTimeCalculationBrowserTest::AllowlistUrlRegx(const std::string& url) {
  builder_.AppendToAllowlistUrlList(url);
  UpdatePolicy();
}

Browser* WebTimeCalculationBrowserTest::DetachTabToNewBrowser(Browser* browser,
                                                              int tab_index) {
  std::vector<int> tabs{tab_index};

  browser->tab_strip_model()->delegate()->MoveTabsToNewWindow(tabs);
  return BrowserList::GetInstance()->GetLastActive();
}

content::WebContents* WebTimeCalculationBrowserTest::Navigate(
    Browser* browser,
    const std::string& url_in,
    WindowOpenDisposition disposition) {
  GURL url =
      embedded_test_server()->GetURL(url_in, "/supervised_user/simple.html");
  NavigateParams params(browser, url, ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  ui_test_utils::NavigateToURL(&params);
  return params.navigated_or_inserted_contents;
}

app_time::ChromeAppActivityState
WebTimeCalculationBrowserTest::GetChromeAppActivityState() {
  auto* service = ChildUserServiceFactory::GetForBrowserContext(profile_);
  ChildUserService::TestApi test_api(service);
  auto* web_time_activity_provider =
      test_api.app_time_controller()->web_time_activity_provider();
  return web_time_activity_provider->chrome_app_activty_state();
}

void WebTimeCalculationBrowserTest::UpdatePolicy() {
  std::string policy_value;
  base::JSONWriter::Write(builder_.value(), &policy_value);

  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_perapptimelimitsallowlist()
      ->set_value(policy_value);

  logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
      profile_);
}

IN_PROC_BROWSER_TEST_F(WebTimeCalculationBrowserTest, TabSelectionChanges) {
  AllowlistUrlRegx(kExampleHost1);

  Navigate(browser(), kExampleHost1, WindowOpenDisposition::CURRENT_TAB);

  // Create a new tab and navigate it to some url.
  Navigate(browser(), kExampleHost2, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  EXPECT_EQ(app_time::ChromeAppActivityState::kActive,
            GetChromeAppActivityState());

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(app_time::ChromeAppActivityState::kActiveAllowlisted,
            GetChromeAppActivityState());

  bool destroyed = browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_USER_GESTURE);
  EXPECT_TRUE(destroyed);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(app_time::ChromeAppActivityState::kActive,
            GetChromeAppActivityState());
}

IN_PROC_BROWSER_TEST_F(WebTimeCalculationBrowserTest, TabDetached) {
  AllowlistUrlRegx(kExampleHost1);

  Navigate(browser(), kExampleHost1, WindowOpenDisposition::CURRENT_TAB);

  // Create a new tab and navigate it to some url.
  Navigate(browser(), kExampleHost2, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  browser()->tab_strip_model()->ActivateTabAt(0);

  EXPECT_EQ(app_time::ChromeAppActivityState::kActiveAllowlisted,
            GetChromeAppActivityState());

  Browser* new_browser = DetachTabToNewBrowser(browser(), 1);

  EXPECT_EQ(app_time::ChromeAppActivityState::kActive,
            GetChromeAppActivityState());

  // Now we have two browser windows. One hosting a allowlisted url and the
  // other hosting a non allowlisted url.
  EXPECT_TRUE(new_browser->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_USER_GESTURE));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(app_time::ChromeAppActivityState::kActiveAllowlisted,
            GetChromeAppActivityState());

  EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CloseTypes::CLOSE_USER_GESTURE));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(app_time::ChromeAppActivityState::kInactive,
            GetChromeAppActivityState());
}

// TODO(yilkal): Write test to check that going to a URL in the current tab of
// the first browser will result in chrome being active or active allowlisted.

}  // namespace ash
