// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/buildflags.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/release_notes/release_notes_notification.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_discover_tab_notification.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kHelpAppDiscoverTabNotificationAllChannels,
         features::kReleaseNotesNotificationAllChannels,
         features::kHelpAppLauncherSearch},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using HelpAppAllProfilesIntegrationTest = HelpAppIntegrationTest;

content::WebContents* GetActiveWebContents() {
  return chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
}

// Waits for and expects that the correct url is opened.
void WaitForAppToOpen(const GURL& expected_url) {
  // Start with a number of browsers (may include an incognito browser).
  size_t num_browsers = chrome::GetTotalBrowserCount();
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be another browser window for the newly opened app.
  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  // Help app should have opened at the expected page.
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

}  // namespace

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::HELP, url, "Explore"));
}

// Test that the Help App is searchable by additional strings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2SearchInLauncher) {
  WaitForTestSystemAppInstall();
  auto* system_app = GetManager().GetSystemApp(web_app::SystemAppType::HELP);
  std::vector<int> search_terms = system_app->GetAdditionalSearchTerms();
  std::vector<std::string> search_terms_strings;
  std::transform(search_terms.begin(), search_terms.end(),
                 std::back_inserter(search_terms_strings),
                 [](int term) { return l10n_util::GetStringUTF8(term); });
  EXPECT_EQ(std::vector<std::string>({"Get Help", "Perks", "Offers"}),
            search_terms_strings);
}

// Test that the Help App has a minimum window size of 600x320.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2MinWindowSize) {
  WaitForTestSystemAppInstall();
  auto* system_app = GetManager().GetSystemApp(web_app::SystemAppType::HELP);
  EXPECT_EQ(system_app->GetMinimumWindowSize(), gfx::Size(600, 320));
}

// Test that the Help App has a default size of 960x600 and is in the center of
// the screen.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DefaultWindowBounds) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(web_app::SystemAppType::HELP, &browser);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  int x = (work_area.width() - 960) / 2;
  int y = (work_area.height() - 600) / 2;
  EXPECT_EQ(browser->window()->GetBounds(), gfx::Rect(x, y, 960, 600));
}

// Test that the Help App logs metric when launching the app using the
// AppServiceProxy.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2AppServiceMetrics) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Using AppServiceProxy gives more coverage of the launch path and ensures
  // the metric is not recorded twice.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app/"));
  navigation_observer.StartWatchingNewWebContents();

  proxy->Launch(
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::HELP),
      ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromKeyboard,
      apps::MakeWindowInfo(display::kDefaultDisplayId));

  navigation_observer.Wait();
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      1);
}

// Test that the Help App can log metrics in the untrusted frame.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2InAppMetrics) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  base::UserActionTester user_action_tester;

  constexpr char kScript[] = R"(
    chrome.metricsPrivate.recordUserAction("Discover.Help.TabClicked");
  )";

  EXPECT_EQ(0, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
}

IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppV2ShowHelp) {
  WaitForTestSystemAppInstall();

  chrome::ShowHelp(browser(), chrome::HELP_SOURCE_KEYBOARD);

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/")));
#else
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
#endif
}

// Test that launching the Help App's release notes opens the app on the Release
// Notes page.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppV2LaunchReleaseNotes) {
  WaitForTestSystemAppInstall();

  // There should be 1 browser window initially.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // The opened window should be showing the url with attached WebUI.
  // The inner frame should be the pathname for the release notes pathname.
  EXPECT_EQ("chrome-untrusted://help-app/updates",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                GetActiveWebContents(), "window.location.href"));
#else
  // Nothing should happen on non-branded builds.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that launching the Help App's release notes logs metrics.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ReleaseNotesMetrics) {
  WaitForTestSystemAppInstall();

  base::UserActionTester user_action_tester;
  chrome::LaunchReleaseNotes(profile(),
                             apps::mojom::LaunchSource::kFromOtherApp);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that clicking the release notes notification opens Help App.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2LaunchReleaseNotesFromNotification) {
  WaitForTestSystemAppInstall();
  base::UserActionTester user_action_tester;
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  auto release_notes_notification =
      std::make_unique<ReleaseNotesNotification>(profile());
  auto release_notes_storage = std::make_unique<ReleaseNotesStorage>(profile());

  // Force the release notes notification to show up.
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  release_notes_notification->MaybeShowReleaseNotes();
  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ("show_release_notes_notification", notifications[0].id());
  // Then click.
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 "show_release_notes_notification",
                                 absl::nullopt, absl::nullopt);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("ReleaseNotes.NotificationShown"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ReleaseNotes.LaunchedNotification"));
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/updates")));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that discover tab notification shows and has functional interactions.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2DiscoverTabNotification) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  base::UserActionTester user_action_tester;
  profile()->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_users::kChildAccountSUID);
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kDiscoverTabSuggestionChipTimesLeftToShow),
            0);

  // Script that simulates what the Help App background page would do to show
  // the discover notification.
  constexpr char kScript[] = R"(
    (async () => {
      await window.customLaunchData.delegate.maybeShowDiscoverNotification();
      window.domAutomationController.send(true);
    })();
  )";
  // Use ExecuteScript instead of EvalJsInAppFrame because the script needs to
  // run in the same world as the page's code.
  bool script_finished;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &script_finished));
  EXPECT_TRUE(script_finished);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kDiscoverTabSuggestionChipTimesLeftToShow),
            3);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  // Close the web contents we just created to simulate what would happen in
  // production with a background page. This helps us ensure that our
  // notification shows up and can be interacted with even after the web ui
  // that triggered it has died.
  web_contents->Close();
  // Wait until the browser with the web contents closes.
  ui_test_utils::WaitForBrowserToClose(browser);

  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ(kShowHelpAppDiscoverTabNotificationId, notifications[0].id());

  // Click on the notification.
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 kShowHelpAppDiscoverTabNotificationId,
                                 absl::nullopt, absl::nullopt);

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/discover")));
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that the background page can trigger the release notes notification.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2ReleaseNotesNotificationFromBackground) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  base::UserActionTester user_action_tester;
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            0);

  // Script that simulates what the Help App background page would do to show
  // the release notes notification.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.maybeShowReleaseNotesNotification();
      window.domAutomationController.send(true);
    })();
  )";
  // Use ExecuteScript instead of EvalJsInAppFrame because the script needs to
  // run in the same world as the page's code.
  bool script_finished;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &script_finished));
  EXPECT_TRUE(script_finished);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            3);

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  // Close the web contents we just created to simulate what would happen in
  // production with a background page. This helps us ensure that our
  // notification shows up and can be interacted with even after the web ui
  // that triggered it has died.
  web_contents->Close();
  // Wait until the browser with the web contents closes.
  ui_test_utils::WaitForBrowserToClose(browser);

  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ("show_release_notes_notification", notifications[0].id());

  // Click on the notification.
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 "show_release_notes_notification",
                                 absl::nullopt, absl::nullopt);

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(WaitForAppToOpen(GURL("chrome://help-app/updates")));
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that the Help App does a navigation on launch even when it was already
// open with the same URL.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2NavigateOnRelaunch) {
  WaitForTestSystemAppInstall();

  // There should initially be a single browser window.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(web_app::SystemAppType::HELP, &browser);

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(web_contents);
  LaunchAppWithoutWaiting(web_app::SystemAppType::HELP);
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // LaunchApp should navigate the existing window and not open any new windows.
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

// Test direct navigation to a subpage.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DirectNavigation) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::HELP);
  params.override_url = GURL("chrome://help-app/help/");

  content::WebContents* web_contents = LaunchApp(std::move(params));

  // The inner frame should have the same pathname as the launch URL.
  EXPECT_EQ("chrome-untrusted://help-app/help/",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
}

// Test that the Help App can open the feedback dialog.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2OpenFeedbackDialog) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // Script that tells the Help App to open the feedback dialog.
  constexpr char kScript[] = R"(
    (async () => {
      const res = await window.customLaunchData.delegate.openFeedbackDialog();
      window.domAutomationController.send(res === null);
    })();
  )";
  bool error_is_null;
  // Use ExecuteScript instead of EvalJsInAppFrame because the script needs to
  // run in the same world as the page's code.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &error_is_null));
  // A null string result means no error in opening feedback.
  EXPECT_TRUE(error_is_null);
}

// Test that the Help App opens the OS Settings family link page.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ShowParentalControls) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://os-settings/osPeople");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  // Script that tells the Help App to show parental controls.
  constexpr char kScript[] = R"(
    (async () => {
      await window.customLaunchData.delegate.showParentalControls();
    })();
  )";
  // Trigger the script, then wait for settings to open. Use ExecuteScript
  // instead of EvalJsInAppFrame because the script needs to run in the same
  // world as the page's code.
  EXPECT_TRUE(content::ExecuteScript(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  navigation_observer.Wait();

  // Settings should be active in a new window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App delegate can update the index for launcher search.
// Also test searching using the help app search handler.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2UpdateLauncherSearchIndexAndSearch) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // Script that adds a data item to the launcher search index.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.updateLauncherSearchIndex([{
        id: 'test-id',
        title: 'Title',
        mainCategoryName: 'Help',
        tags: ['verycomplicatedsearchquery'],
        urlPathWithParameters: 'help/sub/3399763/id/6318213',
        locale: ''
      }]);
      window.domAutomationController.send(true);
    })();
  )";

  bool script_finished;
  // Use ExtractBool to make the script wait until the update completes.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &script_finished));
  EXPECT_TRUE(script_finished);

  // Search using the search handler to confirm that the update happened.
  base::RunLoop run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/3u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     EXPECT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "test-id");
                     EXPECT_EQ(search_results[0]->title, u"Title");
                     EXPECT_EQ(search_results[0]->main_category, u"Help");
                     EXPECT_EQ(search_results[0]->locale, "");
                     EXPECT_GT(search_results[0]->relevance_score, 0.01);
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();
}

// Test that the Help App delegate filters out invalid search concepts when
// updating the launcher search index.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2UpdateLauncherSearchIndexFilterInvalid) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents = LaunchApp(web_app::SystemAppType::HELP);

  // Script that adds a data item to the launcher search index.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.updateLauncherSearchIndex([
        {
          id: '6318213',  // Fix connection problems.
          title: 'Article 1: Invalid',
          mainCategoryName: 'Help',
          tags: ['verycomplicatedsearchquery'],
          urlPathWithParameters: '',  // Invalid because empty field.
          locale: '',
        },
        {
          id: 'test-id-2',
          title: 'Article 2: Valid',
          mainCategoryName: 'Help',
          tags: ['verycomplicatedsearchquery'],
          urlPathWithParameters: 'help/',
          locale: '',
        },
        {
          id: '1700055',  // Open, save, or delete files.
          title: 'Article 3: Invalid',
          mainCategoryName: 'Help',
          tags: [''],  // Invalid because no non-empty tags.
          urlPathWithParameters: 'help/',
          locale: '',
        },
      ]);
      window.domAutomationController.send(true);
    })();
  )";

  bool script_finished;
  // Use ExtractBool to make the script wait until the update completes.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript,
      &script_finished));
  EXPECT_TRUE(script_finished);

  // These hash values can be found in the enum in the google-internal histogram
  // file.
  histogram_tester.ExpectBucketCount(
      "Discover.LauncherSearch.InvalidConceptInUpdate", -20424143, 1);
  histogram_tester.ExpectBucketCount(
      "Discover.LauncherSearch.InvalidConceptInUpdate", 395626524, 1);

  // Search using the search handler to confirm that only the valid article was
  // added to the index.
  base::RunLoop run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/3u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     EXPECT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "test-id-2");
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();
}

// Test that the Help App background task works.
// It should open and update the index for launcher search, then close.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2BackgroundTaskUpdatesLauncherSearchIndex) {
  WaitForTestSystemAppInstall();
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);

  const GURL bg_task_url("chrome://help-app/background");
  content::TestNavigationObserver navigation_observer(bg_task_url);
  navigation_observer.StartWatchingNewWebContents();

  // Wait for system apps background tasks to start.
  base::RunLoop run_loop;
  web_app::WebAppProvider::GetForTest(browser()->profile())
      ->system_web_app_manager()
      .on_tasks_started()
      .Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  auto& tasks = GetManager().GetBackgroundTasksForTesting();

  // Find the help app's background task.
  const auto& help_task = std::find_if(
      tasks.begin(), tasks.end(),
      [&bg_task_url](
          const std::unique_ptr<web_app::SystemAppBackgroundTask>& x) {
        return x->url_for_testing() == bg_task_url;
      });
  ASSERT_NE(help_task, tasks.end());

  auto* timer = help_task->get()->get_timer_for_testing();
  EXPECT_EQ(web_app::SystemAppBackgroundTask::INITIAL_WAIT,
            help_task->get()->get_state_for_testing());
  // The "Immediate" timer waits for several minutes, and it's hard to mock time
  // properly in a browser test, so just fire the timer now. We're not testing
  // that base::Timer works.
  timer->FireNow();

  // Wait for the task to launch the background page.
  navigation_observer.Wait();

  // Store web_content while the page is open.
  content::WebContents* web_contents =
      help_task->get()->web_contents_for_testing();
  // Wait until the background page closes.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  destroyed_watcher.Wait();

  EXPECT_EQ(help_task->get()->opened_count_for_testing(), 1u);

#if !BUILDFLAG(ENABLE_CROS_HELP_APP)
  // This part only works in non-branded builds because it uses fake data added
  // by the mock app.
  // Search using the search handler to confirm that the update happened.
  base::RunLoop search_run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/1u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     ASSERT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "mock-app-test-id");
                     EXPECT_EQ(search_results[0]->title, u"Title");
                     EXPECT_EQ(search_results[0]->main_category, u"Help");
                     EXPECT_EQ(search_results[0]->locale, "");
                     EXPECT_EQ(search_results[0]->url_path_with_parameters,
                               "help/sub/3399763/");
                     EXPECT_GT(search_results[0]->relevance_score, 0.01);
                     search_run_loop.QuitClosure().Run();
                   }));
  search_run_loop.Run();
#endif
}

// Test that the Help App opens when Gesture help requested.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppOpenGestures) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  SystemTrayClientImpl::Get()->ShowGestureEducationHelp();

  EXPECT_NO_FATAL_FAILURE(
      WaitForAppToOpen(GURL("chrome://help-app/help/sub/3399710/id/9739838")));
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromOtherApp", 18,
                                      1);
}

// Test that the Help App opens from keyboard shortcut.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppOpenKeyboardShortcut) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // The /? key is OEM_2 on a US standard keyboard.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_OEM_2, /*control=*/true,
      /*shift=*/false, /*alt=*/false, /*command=*/false));

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // Default browser tab and Help app are open.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ("chrome://help-app/", GetActiveWebContents()->GetVisibleURL());
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      1);
#else
  // We just have the one browser. Navigates chrome.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
  // The HELP app is 18, see DefaultAppName in
  // src/chrome/browser/apps/app_service/app_service_metrics.cc
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard", 18,
                                      0);
#endif
}

// Test that the Help App opens in a new window if try to navigate there in a
// browser.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppCapturesBrowserNavigation) {
  WaitForTestSystemAppInstall();
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app"));
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Try to navigate to the help app in the browser.
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "chrome://help-app");
  navigation_observer.Wait();

  // We now have two browsers, one for the chrome window, one for the Help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL("chrome://help-app"), GetActiveWebContents()->GetVisibleURL());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    HelpAppAllProfilesIntegrationTest);

}  // namespace ash
