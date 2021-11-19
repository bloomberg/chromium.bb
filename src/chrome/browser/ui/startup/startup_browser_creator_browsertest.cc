// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/identity_browser_test_base.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_management_policy.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/ui/startup/web_app_url_handling_startup_test_utils.h"
#include "chrome/browser/ui/views/web_apps/web_app_url_handler_intent_picker_dialog_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/url_handler_manager_impl.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"

using web_app::StartupBrowserWebAppUrlHandlingTest;
#endif

using testing::Return;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

using extensions::Extension;
using testing::_;
using web_app::WebAppProvider;

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

const char kAppId[] = "dofnemchnjfeendjmdhaldenaiabpiad";
const char16_t kAppName[] = u"Test App";
const char kStartUrl[] = "https://test.com";

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

  // Find the new browser.
  Browser* other_browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser)
      other_browser = b;
  }
  return other_browser;
}

bool IsWindows10OrNewer() {
#if defined(OS_WIN)
  return base::win::GetVersion() >= base::win::Version::WIN10;
#else
  return false;
#endif
}

void DisableWelcomePages(const std::vector<Profile*>& profiles) {
  for (Profile* profile : profiles)
    profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  // Also disable What's New.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
}

Browser* OpenNewBrowser(Profile* profile) {
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IS_FIRST_RUN);
  creator.Launch(profile, false, nullptr);
  return chrome::FindBrowserWithProfile(profile);
}

Browser* CloseBrowserAndOpenNew(Browser* browser, Profile* profile) {
  browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser);
  return OpenNewBrowser(profile);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

typedef absl::optional<policy::PolicyLevel> PolicyVariant;

// This class waits until all browser windows are closed, and then runs
// a quit closure.
class AllBrowsersClosedWaiter : public BrowserListObserver {
 public:
  explicit AllBrowsersClosedWaiter(base::OnceClosure quit_closure);
  AllBrowsersClosedWaiter(const AllBrowsersClosedWaiter&) = delete;
  AllBrowsersClosedWaiter& operator=(const AllBrowsersClosedWaiter&) = delete;
  ~AllBrowsersClosedWaiter() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::OnceClosure quit_closure_;
};

AllBrowsersClosedWaiter::AllBrowsersClosedWaiter(base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)) {
  BrowserList::AddObserver(this);
}

AllBrowsersClosedWaiter::~AllBrowsersClosedWaiter() {
  BrowserList::RemoveObserver(this);
}

void AllBrowsersClosedWaiter::OnBrowserRemoved(Browser* browser) {
  if (chrome::GetTotalBrowserCount() == 0)
    std::move(quit_closure_).Run();
}

// This class waits for a specified number of sessions to be restored.
class SessionsRestoredWaiter {
 public:
  explicit SessionsRestoredWaiter(base::OnceClosure quit_closure,
                                  int num_session_restores_expected);
  SessionsRestoredWaiter(const SessionsRestoredWaiter&) = delete;
  SessionsRestoredWaiter& operator=(const SessionsRestoredWaiter&) = delete;
  ~SessionsRestoredWaiter();

 private:
  // Callback for session restore notifications.
  void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

  // For automatically unsubscribing from callback-based notifications.
  base::CallbackListSubscription callback_subscription_;
  base::OnceClosure quit_closure_;
  int num_session_restores_expected_;
  int num_sessions_restored_ = 0;
};

SessionsRestoredWaiter::SessionsRestoredWaiter(
    base::OnceClosure quit_closure,
    int num_session_restores_expected)
    : quit_closure_(std::move(quit_closure)),
      num_session_restores_expected_(num_session_restores_expected) {
  callback_subscription_ = SessionRestore::RegisterOnSessionRestoredCallback(
      base::BindRepeating(&SessionsRestoredWaiter::OnSessionRestoreDone,
                          base::Unretained(this)));
}

SessionsRestoredWaiter::~SessionsRestoredWaiter() = default;

void SessionsRestoredWaiter::OnSessionRestoreDone(Profile* profile,
                                                  int num_tabs_restored) {
  if (++num_sessions_restored_ == num_session_restores_expected_)
    std::move(quit_closure_).Run();
}

}  // namespace

class StartupBrowserCreatorTest : public extensions::ExtensionBrowserTest {
 protected:
  StartupBrowserCreatorTest() {}

  bool SetUpUserDataDirectory() override {
    return extensions::ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kHomePage, url::kAboutBlankURL);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
#endif
  }

  // Helper functions return void so that we can ASSERT*().
  // Use ASSERT_NO_FATAL_FAILURE around calls to these functions to stop the
  // test if an assert fails.
  void LoadApp(const std::string& app_name,
               const Extension** out_app_extension) {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_name.c_str())));

    *out_app_extension = extension_registry()->GetExtensionById(
        last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);
    ASSERT_TRUE(*out_app_extension);

    // Code that opens a new browser assumes we start with exactly one.
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  }

  void SetAppLaunchPref(const std::string& app_id,
                        extensions::LaunchType launch_type) {
    extensions::SetLaunchType(browser()->profile(), app_id, launch_type);
  }

  Browser* FindOneOtherBrowserForProfile(Profile* profile,
                                         Browser* not_this_browser) {
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser != not_this_browser && browser->profile() == profile)
        return browser;
    }
    return nullptr;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if defined(OS_MAC)
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_manager->infobar_count());
#endif  // defined(OS_MAC)
  }
};

class OpenURLsPopupObserver : public BrowserListObserver {
 public:
  OpenURLsPopupObserver() = default;

  void OnBrowserAdded(Browser* browser) override { added_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {}

  Browser* added_browser_ = nullptr;
};

// Test that when there is a popup as the active browser any requests to
// StartupBrowserCreatorImpl::OpenURLsInBrowser don't crash because there's no
// explicit profile given.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost"));

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  OpenURLsPopupObserver observer;
  BrowserList::AddObserver(&observer);

  Browser* popup = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  ASSERT_TRUE(popup->is_type_popup());
  ASSERT_EQ(popup, observer.added_browser_);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a null or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// We don't do non-process-startup browser launches on ChromeOS.
// Session restore for process-startup browser launches is tested
// in session_restore_uitest.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Verify that startup URLs are honored when the process already exists but has
// no tabbed browser windows (eg. as if the process is running only due to a
// background application.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindowWithNoTabbedBrowsers) {
  // Use a couple same-site HTTP URLs.
  ASSERT_TRUE(embedded_test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  urls.push_back(embedded_test_server()->GetURL("/title2.html"));

  Profile* profile = browser()->profile();

  DisableWelcomePages({profile});

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile, pref);

  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  Browser* new_browser = OpenNewBrowser(profile);
  ASSERT_TRUE(new_browser);

  std::vector<GURL> expected_urls(urls);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(expected_urls.size()), tab_strip->count());
  for (size_t i = 0; i < expected_urls.size(); i++)
    EXPECT_EQ(expected_urls[i], tab_strip->GetWebContentsAt(i)->GetURL());

  // The two test_server tabs, despite having the same site, should be in
  // different SiteInstances.
  EXPECT_NE(
      tab_strip->GetWebContentsAt(tab_strip->count() - 2)->GetSiteInstance(),
      tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetSiteInstance());
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindow) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  DisableWelcomePages({browser()->profile()});

  Browser* new_browser = OpenNewBrowser(browser()->profile());
  ASSERT_TRUE(new_browser);

  // The new browser should have exactly one tab (not the startup URLs).
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppUrlShortcut) {
  // Add --app=<url> to the command line. Tests launching legacy apps which may
  // have been created by "Add to Desktop" in old versions of Chrome.
  // TODO(mgiuca): Delete this feature (https://crbug.com/751029). We are
  // keeping it for now to avoid disrupting existing workflows.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  command_line.AppendSwitchASCII(switches::kApp, url.spec());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // The new window should be an app window.
  EXPECT_TRUE(new_browser->is_type_app());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  // At this stage, the web contents' URL should be the one passed in to --app
  // (but it will not yet be committed into the navigation controller).
  EXPECT_EQ("title2.html", web_contents->GetVisibleURL().ExtractFileName());

  // Wait until the navigation is complete. Then the URL will be committed to
  // the navigation controller.
  content::TestNavigationObserver observer(web_contents, 1);
  observer.Wait();
  EXPECT_EQ("title2.html",
            web_contents->GetLastCommittedURL().ExtractFileName());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppUrlIncognitoShortcut) {
  // Add --app=<url> and --incognito to the command line. Tests launching
  // legacy apps which may have been created by "Add to Desktop" in old versions
  // of Chrome. Some existing workflows (especially testing scenarios) also
  // use the --incognito command line.
  // TODO(mgiuca): Delete this feature (https://crbug.com/751029). We are
  // keeping it for now to avoid disrupting existing workflows.
  // IMPORTANT NOTE: This is being committed because it is an easy fix, but
  // this use case is not officially supported. If a future refactor or
  // feature launch causes this to break again, we have no formal
  // responsibility to make this continue working. If you rely on the
  // combination of these two flags, you WILL be broken in the future.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  command_line.AppendSwitchASCII(switches::kApp, url.spec());
  command_line.AppendSwitch(switches::kIncognito);

  Browser* incognito = CreateIncognitoBrowser();

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      incognito->profile(), {}));

  Browser* new_browser = FindOneOtherBrowser(incognito);
  ASSERT_TRUE(new_browser);

  // The new window should be an app window.
  EXPECT_TRUE(new_browser->is_type_app());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  // At this stage, the web contents' URL should be the one passed in to --app
  // (but it will not yet be committed into the navigation controller).
  EXPECT_EQ("title2.html", web_contents->GetVisibleURL().ExtractFileName());

  // Wait until the navigation is complete. Then the URL will be committed to
  // the navigation controller.
  content::TestNavigationObserver observer(web_contents, 1);
  observer.Wait();
  EXPECT_EQ("title2.html",
            web_contents->GetLastCommittedURL().ExtractFileName());
}

namespace {

enum class ChromeAppDeprecationFeatureValue {
  kDefault,
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
  kEnabled,
  kDisabled,
#endif
};

enum class ChromeAppsEnabledPrefValue {
  kDefault,
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
  kEnabled,
  kDisabled,
#endif
};

std::string ChromeAppDeprecationFeatureValueToString(
    const ::testing::TestParamInfo<std::tuple<ChromeAppDeprecationFeatureValue,
                                              ChromeAppsEnabledPrefValue>>&
        param_info) {
  std::string result;
  switch (std::get<0>(param_info.param)) {
    case ChromeAppDeprecationFeatureValue::kDefault:
      result = "ChromeAppDeprecationFeatureDefault";
      break;
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    case ChromeAppDeprecationFeatureValue::kEnabled:
      result = "ChromeAppDeprecationFeatureEnabled";
      break;
    case ChromeAppDeprecationFeatureValue::kDisabled:
      result = "ChromeAppDeprecationFeatureDisabled";
      break;
#endif
  }
  result += '_';
  switch (std::get<1>(param_info.param)) {
    case ChromeAppsEnabledPrefValue::kDefault:
      result += "ChromeAppsEnabledPrefDefault";
      break;
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    case ChromeAppsEnabledPrefValue::kEnabled:
      result += "ChromeAppsEnabledPrefEnabled";
      break;
    case ChromeAppsEnabledPrefValue::kDisabled:
      result += "ChromeAppsEnabledPrefDisabled";
      break;
#endif
  }
  return result;
}

}  // namespace

class StartupBrowserCreatorChromeAppShortcutTest
    : public StartupBrowserCreatorTest,
      public ::testing::WithParamInterface<
          std::tuple<ChromeAppDeprecationFeatureValue,
                     ChromeAppsEnabledPrefValue>> {
 protected:
  StartupBrowserCreatorChromeAppShortcutTest() {
    switch (std::get<0>(GetParam())) {
      case ChromeAppDeprecationFeatureValue::kDefault:
        break;
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
      case ChromeAppDeprecationFeatureValue::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(
            features::kChromeAppsDeprecation);
        break;
      case ChromeAppDeprecationFeatureValue::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(
            features::kChromeAppsDeprecation);
        break;
#endif
    }
  }

  void SetUpOnMainThread() override {
    StartupBrowserCreatorTest::SetUpOnMainThread();
    switch (std::get<1>(GetParam())) {
      case ChromeAppsEnabledPrefValue::kDefault:
        break;
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
      case ChromeAppsEnabledPrefValue::kEnabled:
        browser()->profile()->GetPrefs()->SetBoolean(
            extensions::pref_names::kChromeAppsEnabled, true);
        break;
      case ChromeAppsEnabledPrefValue::kDisabled:
        browser()->profile()->GetPrefs()->SetBoolean(
            extensions::pref_names::kChromeAppsEnabled, false);
        break;
#endif
    }
  }

  void ExpectBlockLaunch() {
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // Should have opened the requested homepage about:blank in 1st window.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    EXPECT_EQ(1, tab_strip->count());
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
    // Should have opened the chrome://apps unsupported app flow in 2nd window.
    Browser* other_browser = FindOneOtherBrowser(browser());
    ASSERT_TRUE(other_browser);
    TabStripModel* other_tab_strip = other_browser->tab_strip_model();
    EXPECT_EQ(1, other_tab_strip->count());
    EXPECT_FALSE(other_browser->is_type_app());
    EXPECT_TRUE(other_browser->is_type_normal());
    EXPECT_EQ(GURL(chrome::kChromeUIAppsURL),
              other_tab_strip->GetWebContentsAt(0)->GetURL());
  }

  bool IsExpectedToAllowLaunch() {
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    switch (std::get<1>(GetParam())) {
      case ChromeAppsEnabledPrefValue::kEnabled:
        return true;
      case ChromeAppsEnabledPrefValue::kDisabled:
      case ChromeAppsEnabledPrefValue::kDefault:
        break;
    }
    switch (std::get<0>(GetParam())) {
      case ChromeAppDeprecationFeatureValue::kEnabled:
        return false;
      case ChromeAppDeprecationFeatureValue::kDefault:
      case ChromeAppDeprecationFeatureValue::kDisabled:
        return true;
    }
#endif
    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  if (IsExpectedToAllowLaunch()) {
    // No pref was set, so the app should have opened in a tab in the existing
    // window.
    tab_waiter.Wait();
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(tab_strip->GetActiveWebContents(),
              tab_strip->GetWebContentsAt(1));

    // It should be a standard tabbed window, not an app window.
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());

    // It should have loaded the requested app.
    const std::u16string expected_title(
        u"app_with_tab_container/empty.html title");
    content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                        expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    ExpectBlockLaunch();
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutWindowPref) {
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  if (IsExpectedToAllowLaunch()) {
    // Pref was set to open in a window, so the app should have opened in a
    // window.  The launch should have created a new browser. Find the new
    // browser.
    Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
    ASSERT_TRUE(new_browser);

    // Expect an app window.
    EXPECT_TRUE(new_browser->is_type_app());

    // The browser's app_name should include the app's ID.
    EXPECT_NE(new_browser->app_name().find(extension_app->id()),
              std::string::npos)
        << new_browser->app_name();
  } else {
    ExpectBlockLaunch();
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutTabPref) {
  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a tab.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  if (IsExpectedToAllowLaunch()) {
    // When an app shortcut is open and the pref indicates a tab should open,
    // the tab is open in the existing browser window.
    tab_waiter.Wait();
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(tab_strip->GetActiveWebContents(),
              tab_strip->GetWebContentsAt(1));

    // The browser's app_name should not include the app's ID: it is in a normal
    // tabbed browser.
    EXPECT_EQ(browser()->app_name().find(extension_app->id()),
              std::string::npos)
        << browser()->app_name();

    // It should have loaded the requested app.
    const std::u16string expected_title(
        u"app_with_tab_container/empty.html title");
    content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                        expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    ExpectBlockLaunch();
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenPolicyForcedAppShortcut) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Install a test policy provider which will mark the app as force-installed.
  extensions::TestManagementPolicyProvider policy_provider(
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(browser()->profile());
  extension_system->management_policy()->RegisterProvider(&policy_provider);

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));
  tab_waiter.Wait();

  // Policy force-installed app should be allowed regardless of Chrome App
  // Deprecation status.
  //
  // No app launch pref was set, so the app should have opened in a tab in the
  // existing window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab_strip->GetWebContentsAt(1));

  // It should be a standard tabbed window, not an app window.
  EXPECT_FALSE(browser()->is_type_app());
  EXPECT_TRUE(browser()->is_type_normal());

  // It should have loaded the requested app.
  const std::u16string expected_title(
      u"app_with_tab_container/empty.html title");
  content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                      expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorChromeAppShortcutTest,
    ::testing::Combine(
        ::testing::Values(ChromeAppDeprecationFeatureValue::kDefault
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
                          ,
                          ChromeAppDeprecationFeatureValue::kEnabled,
                          ChromeAppDeprecationFeatureValue::kDisabled
#endif
                          ),
        ::testing::Values(ChromeAppsEnabledPrefValue::kDefault
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
                          ,
                          ChromeAppsEnabledPrefValue::kEnabled,
                          ChromeAppsEnabledPrefValue::kDisabled
#endif
                          )),
    ChromeAppDeprecationFeatureValueToString);

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ValidNotificationLaunchId) {
  // Simulate a launch from the notification_helper process which appends the
  // kNotificationLaunchId switch to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  // The launch delegates to the notification system and doesn't open any new
  // browser window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, InvalidNotificationLaunchId) {
  // Simulate a launch with invalid launch id, which will fail.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kNotificationLaunchId, L"");
  StartupBrowserCreator browser_creator;
  ASSERT_FALSE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  // No new browser window is open.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       NotificationLaunchIdDisablesLastOpenProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    other_profile = profile_manager->GetProfile(dest_path);
  }
  ASSERT_TRUE(other_profile);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Simulate a launch.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");

  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(other_profile);

  StartupBrowserCreator browser_creator;
  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 1.
  ASSERT_EQ(1u, chrome::GetBrowserCount(default_profile));

  // When the kNotificationLaunchId switch is present, any last opened profile
  // is ignored. Thus there is no browser for other_profile.
  ASSERT_EQ(0u, chrome::GetBrowserCount(other_profile));
}

#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterRestart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterNormalStart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, false);
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsForTwoProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    other_profile = profile_manager->GetProfile(dest_path);
  }
  ASSERT_TRUE(other_profile);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(default_profile, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(other_profile, pref2);

  DisableWelcomePages({default_profile, other_profile});

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // urls1 were opened in a browser for default_profile, and urls2 were opened
  // in a browser for other_profile.
  Browser* new_browser = nullptr;
  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 2.
  ASSERT_EQ(2u, chrome::GetBrowserCount(default_profile));
  new_browser = FindOneOtherBrowserForProfile(default_profile, browser());
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the desired URL for the profile.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetURL());

  ASSERT_EQ(1u, chrome::GetBrowserCount(other_profile));
  new_browser = FindOneOtherBrowserForProfile(other_profile, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls2[0], tab_strip->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, PRE_UpdateWithTwoProfiles) {
  // Simulate a browser restart by creating the profiles in the PRE_ part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }
  DisableWelcomePages({profile1, profile2});

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile2, true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));
  CloseBrowserSynchronously(browser2);

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, UpdateWithTwoProfiles) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  // Simulate a launch after a browser update.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);

  base::RunLoop run_loop;
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile1,
                        last_opened_profiles);
  run_loop.Run();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 4 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));
  base::FilePath dest_path4 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 4"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile_home1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile_home1);
  Profile* profile_home2 = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile_home2);
  Profile* profile_last = profile_manager->GetProfile(dest_path3);
  ASSERT_TRUE(profile_last);
  Profile* profile_urls = profile_manager->GetProfile(dest_path4);
  ASSERT_TRUE(profile_urls);

  DisableWelcomePages(
      {profile_home1, profile_home2, profile_last, profile_urls});

  // Set the profiles to open urls, open last visited pages or display the home
  // page.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home1, pref_home);
  SessionStartupPref::SetStartupPref(profile_home2, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Open a page with profile_last.
  Browser* browser_last = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile_last, true));
  chrome::NewTab(browser_last);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_last, embedded_test_server()->GetURL("/empty.html")));

  // Close the browser without deleting |profile_last|.
  ScopedProfileKeepAlive profile_last_keep_alive(
      profile_last, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser_last);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home1);
  last_opened_profiles.push_back(profile_home2);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);

  base::RunLoop run_loop;
  // Only profile_last should get its session restored.
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home1,
                        last_opened_profiles);
  run_loop.Run();

  Browser* new_browser = nullptr;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP (and the welcome page on
  // relevant platforms).
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home1));
  new_browser = FindOneOtherBrowserForProfile(profile_home1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(new_browser->profile()),
            tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_urls opened the urls.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(profile_home2));
}

// This tests that opening multiple profiles with session restore enabled,
// shutting down, and then launching with kNoStartupWindow doesn't restore
// the previously opened profiles.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(https://crbug.com/1196684): enable this test on Lacros.
#define MAYBE_RestoreWithNoStartupWindow DISABLED_RestoreWithNoStartupWindow
#else
#define MAYBE_RestoreWithNoStartupWindow RestoreWithNoStartupWindow
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       MAYBE_RestoreWithNoStartupWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 2 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile1);
  Profile* profile2 = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile2);

  DisableWelcomePages({profile1, profile2});

  // Set the profiles to open last visited pages.
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile1, pref_last);
  SessionStartupPref::SetStartupPref(profile2, pref_last);

  Profile* default_profile = browser()->profile();

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose if
  // needed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive default_profile_keep_alive(
      default_profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open a page with profile1 and profile2.
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, profile2, true});
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/empty.html")));
  // Exit the browser, saving the multi-profile session state.
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  {
    base::RunLoop run_loop;
    AllBrowsersClosedWaiter waiter(run_loop.QuitClosure());
    run_loop.Run();
  }

#if defined(OS_MAC)
  // While we closed all the browsers above, this doesn't quit the Mac app,
  // leaving the app in a half-closed state. Cancel the termination to put the
  // Mac app back into a known state.
  chrome_browser_application_mac::CancelTerminate();
#endif

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles = {profile1, profile2};
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // TODO(davidbienvenu): Waiting for some sort of browser is started
  // notification would be better. But, we're not opening any browser
  // windows, so we'd need to invent a new notification.
  content::RunAllTasksUntilIdle();

  // No browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(profile1), 0u);
  EXPECT_EQ(chrome::GetBrowserCount(profile2), 0u);

  base::CommandLine empty(base::CommandLine::NO_PROGRAM);
  base::RunLoop run_loop;
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(empty, {},
                                                          dest_path1);
  run_loop.Run();

  // profile1 and profile2 browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(profile1), 1u);
  EXPECT_EQ(chrome::GetBrowserCount(profile2), 1u);
}

// Flaky. See https://crbug.com/819976.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       DISABLED_ProfilesLaunchedAfterCrash) {
  // After an unclean exit, all profiles will be launched. However, they won't
  // open any pages automatically.

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 3 profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));

  Profile* profile_home = nullptr;
  Profile* profile_last = nullptr;
  Profile* profile_urls = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile_home = profile_manager->GetProfile(dest_path1);
    ASSERT_TRUE(profile_home);
    profile_last = profile_manager->GetProfile(dest_path2);
    ASSERT_TRUE(profile_last);
    profile_urls = profile_manager->GetProfile(dest_path3);
    ASSERT_TRUE(profile_urls);
  }

  // Set the profiles to open the home page, last visited pages or URLs.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Simulate a launch after an unclear exit.
  CloseBrowserAsynchronously(browser());
  ExitTypeService::GetInstanceForProfile(profile_home)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(profile_last)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(profile_urls)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

#if !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use HistogramTester to make sure a bubble is shown when it's not on
  // platform Mac OS X and it's not official Chrome build.
  //
  // On Mac OS X, an infobar is shown to restore the previous session, which
  // is tested by function EnsureRestoreUIWasShown.
  //
  // Under a Google Chrome build, it is not tested because a task is posted to
  // the file thread before the bubble is shown. It is difficult to make sure
  // that the histogram check runs after all threads have finished their tasks.
  base::HistogramTester histogram_tester;
#endif  // !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home,
                        last_opened_profiles);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  // The welcome page is also shown for relevant platforms.
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home));
  new_browser = FindOneOtherBrowserForProfile(profile_home, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

#if !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Each profile should have one session restore bubble shown, so we should
  // observe count 3 in bucket 0 (which represents bubble shown).
  histogram_tester.ExpectBucketCount("SessionCrashed.Bubble", 0, 3);
#endif  // !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       LaunchMultipleLockedProfiles) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));
    profile2 = profile_manager->GetProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("New Profile 2")));
  }
  ASSERT_TRUE(profile1);
  ASSERT_TRUE(profile2);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile2, pref);

  ProfileAttributesEntry* entry1 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile1->GetPath());
  ASSERT_NE(entry1, nullptr);
  entry1->LockForceSigninProfile(true);

  ProfileAttributesEntry* entry2 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile2->GetPath());
  ASSERT_NE(entry2, nullptr);
  entry2->LockForceSigninProfile(false);

  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        profile1, last_opened_profiles);

  ASSERT_EQ(0u, chrome::GetBrowserCount(profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
web_app::AppId InstallPWA(Profile* profile, const GURL& start_url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

class StartupBrowserCreatorRestartTest : public StartupBrowserCreatorTest,
                                         public BrowserListObserver {
 protected:
  StartupBrowserCreatorRestartTest() { BrowserList::AddObserver(this); }
  ~StartupBrowserCreatorRestartTest() override {
    // We might have already been removed but it's safe to call again.
    BrowserList::RemoveObserver(this);
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::StringPiece test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    if (base::StartsWith(test_name, "PRE_")) {
      // The PRE_ test will call chrome::AttemptRestart().
      mock_relaunch_callback_ = std::make_unique<::testing::StrictMock<
          base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>>();
      EXPECT_CALL(*mock_relaunch_callback_, Run);
      relaunch_chrome_override_ =
          std::make_unique<upgrade_util::ScopedRelaunchChromeBrowserOverride>(
              mock_relaunch_callback_->Get());
    }
  }

  void OnBrowserAdded(Browser* browser) override {
    base::StringPiece test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    // The non PRE_ test will start up as if it was restarted.
    // Check that, then remove the observer.
    if (!base::StartsWith(test_name, "PRE_")) {
      EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
      EXPECT_FALSE(browser_added_check_passed_);
      browser_added_check_passed_ = true;
      BrowserList::RemoveObserver(this);
    }
  }

  bool browser_added_check_passed_ = false;

 private:
  std::unique_ptr<
      base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>
      mock_relaunch_callback_;
  std::unique_ptr<upgrade_util::ScopedRelaunchChromeBrowserOverride>
      relaunch_chrome_override_;
};

// Open an App and restart in preparation for the real test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorRestartTest,
                       PRE_ProfileRestartedAppRestore) {
  // Ensure services are started.
  Profile* test_profile = browser()->profile();

  AppSessionServiceFactory::GetForProfileForSessionRestore(test_profile);
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(test_profile, pref_last);

  // Install web app
  auto example_url = GURL("http://www.example.com");
  web_app::AppId app_id = InstallPWA(test_profile, example_url);
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(test_profile, app_id);

  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  chrome::AttemptRestart();

  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
}

// This test tests a specific scenario where the browser is marked as restarted
// and a SessionBrowserCreatorImpl::MaybeAsyncRestore is triggered.
// ShouldRestoreApps will return true because the profile is marked as
// restarted which will trigger apps to restore. If apps are open at this point
// and an app restore occurs, apps will be duplicated. This test ensures that
// does not occur. This test doesn't build on non app_session_service
// platforms, hence the buildflag disablement.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorRestartTest,
                       ProfileRestartedAppRestore) {
  Profile* test_profile = browser()->profile();

  // StartupBrowserCreator() has already run in SetUp(), so it would already be
  // reset by this point.
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_TRUE(browser_added_check_passed_);
  // Now close the original (and last alive) tabbed browser window
  // note: there is still an app open
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1U, BrowserList::GetInstance()->size());

  // Now hit the codepath that would get hit if someone opened chrome
  // from a desktop shortcut or similar.
  SessionRestoreTestHelper restore_waiter;
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IS_NOT_FIRST_RUN);
  creator.Launch(test_profile, false, nullptr);
  restore_waiter.Wait();

  // We expect a browser to open, but we should NOT get a duplicate app.
  // Note at this point, the profile IsRestarted() is still true.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  bool app_found = false;
  bool browser_found = false;
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      ASSERT_FALSE(app_found);
      app_found = true;
    } else if (browser->type() == Browser::Type::TYPE_NORMAL) {
      ASSERT_FALSE(browser_found);
      browser_found = true;
    }
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

// An observer that returns back to test code after a new browser is added to
// the BrowserList.
class BrowserAddedObserver : public BrowserListObserver {
 public:
  BrowserAddedObserver() { BrowserList::AddObserver(this); }

  ~BrowserAddedObserver() override { BrowserList::RemoveObserver(this); }

  Browser* Wait() {
    run_loop_.Run();
    return browser_;
  }

 protected:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser_ = browser;
    run_loop_.Quit();
  }

 private:
  Browser* browser_ = nullptr;
  base::RunLoop run_loop_;
};

class StartupBrowserWithWebAppTest : public StartupBrowserCreatorTest,
                                     public testing::WithParamInterface<bool> {
 protected:
  StartupBrowserWithWebAppTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kDesktopPWAsFileHandlingSettingsGated, GetParam());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorTest::SetUpCommandLine(command_line);
    if (GetTestPreCount() == 1) {
      // Load an app with launch.container = 'window'.
      command_line->AppendSwitchASCII(switches::kAppId, kAppId);
      command_line->AppendSwitchASCII(switches::kProfileDirectory, "Default");
    }
  }
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserWithWebAppTest,
                       PRE_PRE_LastUsedProfilesWithWebApp) {
  // Simulate a browser restart by creating the profiles in the PRE_PRE part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }
  DisableWelcomePages({profile1, profile2});

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/title1.html")));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, profile2, true});
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/title2.html")));

  // Set startup preferences for the 2 profiles to restore last session.
  SessionStartupPref pref1(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();

  // Install a web app that we will launch from the command line in
  // the PRE test.
  WebAppProvider* const provider =
      WebAppProvider::GetForTest(browser()->profile());
  web_app::WebAppInstallFinalizer& web_app_finalizer =
      provider->install_finalizer();

  web_app::WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;

  // Install web app set to open as a tab.
  {
    base::RunLoop run_loop;
    WebApplicationInfo info;
    info.start_url = GURL(kStartUrl);
    info.title = kAppName;
    info.user_display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_finalizer.FinalizeInstall(
        info, options,
        base::BindLambdaForTesting(
            [&](const web_app::AppId& app_id, web_app::InstallResultCode code) {
              EXPECT_EQ(app_id, kAppId);
              EXPECT_EQ(code, web_app::InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();

    EXPECT_EQ(provider->registrar().GetAppUserDisplayMode(kAppId),
              blink::mojom::DisplayMode::kStandalone);
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserWithWebAppTest,
                       PRE_LastUsedProfilesWithWebApp) {
  BrowserAddedObserver added_observer;
  content::RunAllTasksUntilIdle();
  // Launching with an app opens the app window via a task, so the test
  // might start before SelectFirstBrowser is called.
  if (!browser()) {
    added_observer.Wait();
    SelectFirstBrowser();
  }
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // An app window should have been launched.
  EXPECT_TRUE(browser()->is_type_app());
  CloseBrowserAsynchronously(browser());
}

IN_PROC_BROWSER_TEST_P(StartupBrowserWithWebAppTest,
                       LastUsedProfilesWithWebApp) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  while (SessionRestore::IsRestoring(profile1) ||
         SessionRestore::IsRestoring(profile2)) {
    base::RunLoop().RunUntilIdle();
  }

  // The last open sessions should be restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title1.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title2.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StartupBrowserWithWebAppTest,
                         ::testing::Values(true, false));

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class StartupBrowserWithRealWebAppTest : public StartupBrowserCreatorTest {
 protected:
  StartupBrowserWithRealWebAppTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {}

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       PRE_PRE_LastUsedProfilesWithRealWebApp) {
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  // Simulate a browser restart by creating the profiles in the PRE_PRE part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a profile.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);
  }
  DisableWelcomePages({profile1});

  // Open some urls with the browsers, and close them.
  SessionServiceFactory::GetForProfileForSessionRestore(profile1);
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/title1.html")));
  browser1->window()->Show();
  browser1->window()->Maximize();

  // Set startup preferences to restore last session.
  SessionStartupPref pref1(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile1, pref1);
  profile1->GetPrefs()->CommitPendingWrite();

  SessionStartupPref::SetStartupPref(browser()->profile(), pref1);
  browser()->profile()->GetPrefs()->CommitPendingWrite();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       PRE_LastUsedProfilesWithRealWebApp) {
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile* profile1 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);
  }

  auto example_url = GURL("http://www.example.com");
  web_app::AppId new_app_id = InstallPWA(profile1, example_url);
  Browser* app = web_app::LaunchWebAppBrowserAndWait(profile1, new_app_id);
  ASSERT_TRUE(app);

  // destroy session services so we don't record this closure.
  // This simulates a user choosing ... -> Exit Chromium.
  for (auto* profile : profile_manager->GetLoadedProfiles()) {
    // Don't construct SessionServices for every type just to
    // shut them down. If they were never created, just skip.
    if (SessionServiceFactory::GetForProfileIfExisting(profile))
      SessionServiceFactory::ShutdownForProfile(profile);

    if (AppSessionServiceFactory::GetForProfileIfExisting(profile))
      AppSessionServiceFactory::ShutdownForProfile(profile);
  }

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2u, chrome::GetBrowserCount(profile1));

  // On ozone-linux, for some reason, these profile 1 windows come back in
  // the next test. To reliably ensure they don't, but don't destroy the
  // session restore state, close them while the session services are shutdown.

  Browser* close_this = FindOneOtherBrowserForProfile(profile1, app);
  CloseBrowserSynchronously(close_this);
  CloseBrowserSynchronously(app);
}

#if defined(OS_MAC)
#define MAYBE_LastUsedProfilesWithRealWebApp \
  DISABLED_LastUsedProfilesWithRealWebApp
#else
#define MAYBE_LastUsedProfilesWithRealWebApp LastUsedProfilesWithRealWebApp
#endif
// TODO(stahon@microsoft.com) App restores are disabled on mac.
// see http://crbug.com/1194201
IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       MAYBE_LastUsedProfilesWithRealWebApp) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ASSERT_TRUE(StartupBrowserCreator::WasRestarted());
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* default_profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);
  }
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    default_profile = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("Default")));
    ASSERT_TRUE(profile1);
  }

  // At this point, nothing is open except the basic browser.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Trigger the restore via StartupBrowserCreator.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_NOT_FIRST_RUN);
  // Fake |process_startup| true.
  EXPECT_TRUE(launch.Launch(profile1, /* process_startup */ true, nullptr));

  // We should get two windows from profile1.
  ASSERT_EQ(3u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, chrome::GetBrowserCount(default_profile));
  ASSERT_EQ(2u, chrome::GetBrowserCount(profile1));

  while (SessionRestore::IsRestoring(profile1)) {
    base::RunLoop().RunUntilIdle();
  }

  // Since there's one app being restored, ensure the provider is ready.
  WebAppProvider* provider = WebAppProvider::GetForTest(profile1);
  ASSERT_TRUE(provider->on_registry_ready().is_signaled());

  // The last open sessions should be restored.
  EXPECT_TRUE(profile1->restored_last_session());

  Browser* new_browser = nullptr;

  // 2x profile1, 1x default profile here.
  ASSERT_EQ(3u, BrowserList::GetInstance()->size());
  ASSERT_EQ(2u, chrome::GetBrowserCount(profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(default_profile));
  new_browser = FindOneOtherBrowserForProfile(profile1, nullptr);
  if (new_browser->type() != Browser::Type::TYPE_NORMAL) {
    new_browser = FindOneOtherBrowserForProfile(profile1, new_browser);
  }
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->type(), Browser::Type::TYPE_NORMAL);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title1.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  // Now get the app, it should just be the other browser from this profile.
  new_browser = FindOneOtherBrowserForProfile(profile1, new_browser);
  ASSERT_EQ(new_browser->type(), Browser::Type::TYPE_APP);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// URL Handling tests.
#if defined(OS_WIN) || (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogCancelled_NoLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  SetUpCommandlineAndStart(start_url);

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  // When dialog is closed, nothing will happen.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogAccepted_BrowserLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the first choice, which is the browser.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 0);
  SetUpCommandlineAndStart(start_url);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // When dialog is closed, URL will be launched in a browser window.
  // Check for new browser window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  Browser* other_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(other_browser);
  ASSERT_FALSE(
      web_app::AppBrowserController::IsForWebApp(other_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogAccepted_RememberBrowserLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  base::HistogramTester histogram_tester;

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  auto command_line = SetUpCommandLineWithUrl(start_url);
  // Get matches before dialog launch.
  auto url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line);

  // Select and remember the first choice, which is the browser.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION, 0);
  Start(command_line);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kBrowserAcceptedAndRememberChoice,
      1);

  // When dialog is closed, URL will be launched in a browser window.
  // Check for new browser window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  Browser* other_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(other_browser);
  ASSERT_FALSE(
      web_app::AppBrowserController::IsForWebApp(other_browser, app_id));

  // Get matches after dialog is closed.
  auto new_url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line);
  ASSERT_NE(url_handler_matches, new_url_handler_matches);
  // Verify opening in browser is saved as the default choice (i.e. no matches
  // found).
  ASSERT_TRUE(new_url_handler_matches.empty());

  // Close the browser window and start with the same command line again.
  // Browser should be launched directly.
  CloseBrowserSynchronously(other_browser);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  Start(command_line);
  // Verify browser window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  other_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(other_browser);
  ASSERT_FALSE(
      web_app::AppBrowserController::IsForWebApp(other_browser, app_id));

  // Dialog wasn't shown, the total count of dialog state stays the same.
  histogram_tester.ExpectTotalCount("WebApp.UrlHandling.DialogState", 1);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogAccepted_RememberWebAppLaunch) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  base::HistogramTester histogram_tester;
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  auto command_line = SetUpCommandLineWithUrl(start_url);
  // Get matches before dialog launch.
  auto url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line);

  // Select and remember the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION, 1);
  Start(command_line);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kAppAcceptedAndRememberChoice,
      1);

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(start_url), web_contents->GetVisibleURL());

  // Get matches after dialog is closed.
  auto new_url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line);
  ASSERT_NE(url_handler_matches, new_url_handler_matches);

  // Close the app window and start with the same command line again. App
  // should be launched directly.
  CloseBrowserSynchronously(app_browser);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  Start(command_line);
  ui_test_utils::WaitForBrowserToOpen();
  // Verify app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogAccepted_WebAppLaunch_InScopeUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 1);
  // start_url is in app scope.
  SetUpCommandlineAndStart(start_url);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       DialogAccepted_WebAppLaunch_DifferentOriginUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // Select the second choice, which is the app.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 1);
  // URL is not in app scope but matches url_handlers of installed app.
  constexpr char kTargetUrl[] = "https://example.com/abc/def";
  SetUpCommandlineAndStart(kTargetUrl);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Out-of-scope URL launch should open new app window and navigate to the
  // out-of-scope URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kTargetUrl), web_contents->GetVisibleURL());
}

// TODO(crbug.com/1226532): This test is flaky on Windows. Fix and reenable it.
#if defined(OS_WIN)
#define MAYBE_MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl \
  DISABLED_MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl
#else
#define MAYBE_MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl \
  MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl
#endif

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppUrlHandlingTest,
    MAYBE_MultipleProfiles_DialogAccepted_WebAppLaunch_InScopeUrl) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  // Create profiles and install URL Handling apps.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id_1 = web_app::test::InstallWebAppWithUrlHandlers(
      profile1, GURL(start_url), app_name, {url_handler});
  web_app::AppId app_id_2 = web_app::test::InstallWebAppWithUrlHandlers(
      profile2, GURL(start_url), app_name, {url_handler});

  // Test that we should be able to select the 3rd option.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 2);
  // start_url is in app scope for both apps.
  SetUpCommandlineAndStart(start_url);
  AutoCloseDialog(waiter.WaitIfNeededAndGet());

  // There should be one app window. No deterministic ordering of apps, so find
  // which profile app is launched.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1) +
                    chrome::GetBrowserCount(profile2));
  Profile* app_profile =
      (chrome::GetBrowserCount(profile1) == 1) ? profile1 : profile2;
  Browser* app_browser = chrome::FindBrowserWithProfile(app_profile);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(
      web_app::AppBrowserController::IsForWebApp(app_browser, app_id_1) ||
      web_app::AppBrowserController::IsForWebApp(app_browser, app_id_2));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       CheckHistogramsFired) {
  base::HistogramTester histogram_tester;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppUrlHandlerIntentPickerView");

  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(start_url));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  SetUpCommandlineAndStart(start_url);

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  histogram_tester.ExpectTotalCount(
      "WebApp.UrlHandling.GetValidProfilesAtStartUp", 1);
  histogram_tester.ExpectTotalCount(
      "WebApp.UrlHandling.LoadWebAppRegistrarsAtStartUp", 1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::kClosed, 1);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest, UrlNotCaptured) {
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // This URL is not in scope of installed app and does not match url_handlers.
  SetUpCommandlineAndStart("https://en.example.com/abc/def");

  // Check that new window is not app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  Browser* other_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(other_browser);
  ASSERT_FALSE(
      web_app::AppBrowserController::IsForWebApp(other_browser, app_id));
}
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)

class StartupBrowserWebAppProtocolHandlingTest : public InProcessBrowserTest {
 protected:
  StartupBrowserWebAppProtocolHandlingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableProtocolHandlers);
  }

  bool AreProtocolHandlersSupported() {
#if defined(OS_WIN)
    return base::win::GetVersion() > base::win::Version::WIN7;
#else
    return true;
#endif
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForTest(browser()->profile());
  }

  // Install a web app with `protocol_handlers` (and optionally `file_handlers`)
  // then register it with the ProtocolHandlerRegistry. This is sufficient for
  // testing URL translation and launch at startup.
  web_app::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers,
      const std::vector<apps::FileHandler>& file_handlers = {}) {
    std::unique_ptr<WebApplicationInfo> info =
        std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(kStartUrl);
    info->title = kAppName;
    info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    info->protocol_handlers = protocol_handlers;
    info->file_handlers = file_handlers;
    web_app::AppId app_id =
        web_app::test::InstallWebApp(browser()->profile(), std::move(info));

    auto& protocol_handler_manager =
        provider()
            ->os_integration_manager()
            .protocol_handler_manager_for_testing();

    base::RunLoop run_loop;
    protocol_handler_manager.RegisterOsProtocolHandlers(
        app_id, base::BindLambdaForTesting([&](web_app::Result result) {
          EXPECT_EQ(web_app::Result::kOk, result);
          run_loop.Quit();
        }));
    run_loop.Run();

    return app_id;
  }

  void SetUpCommandlineAndStart(const std::string& url,
                                const web_app::AppId& app_id) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(url);
    command_line.AppendSwitchASCII(switches::kAppId, app_id);

    std::vector<Profile*> last_opened_profiles;
    StartupBrowserCreator browser_creator;
    browser_creator.Start(command_line,
                          g_browser_process->profile_manager()->user_data_dir(),
                          browser()->profile(), last_opened_profiles);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsNotLaunchedWithProtocolUrlAndDialogCancel) {
  if (!AreProtocolHandlersSupported())
    return;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  // Check that no extra window is launched.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithProtocolUrlAndDialogAccept) {
  if (!AreProtocolHandlersSupported())
    return;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  bool allowed_protocols_notified = false;
  web_app::WebAppTestRegistryObserverAdapter observer(browser()->profile());
  observer.SetWebAppProtocolSettingsChangedDelegate(
      base::BindLambdaForTesting([&]() { allowed_protocols_notified = true; }));

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(true);

  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(false);
  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_TRUE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));
  EXPECT_TRUE(allowed_protocols_notified);

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsNotTranslatedWithUnhandledProtocolUrl) {
  if (!AreProtocolHandlersSupported())
    return;

  // Register web app as a protocol handler that should *not* handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with an unhandled protocol URL param.
  SetUpCommandlineAndStart("web+unhandled://parameterString", app_id);

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check an app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched to the home page and not the translated URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithAllowedProtocolUrlPref) {
  if (!AreProtocolHandlersSupported())
    return;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(true);
  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(false);

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_TRUE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));

  // Check the first app window is created.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser1;
  app_browser1 = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser1);

  // Launch the browser via a command line with an handled protocol URL
  // param, but this time we expect the permission dialog to not show up.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check the second app window is launched directly this time. The dialog
  // is skipped because we have the allowed protocol scheme for the same
  // app launch.
  Browser* app_browser2;
  // There should be 3 browser windows opened at the moment.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser() && b != app_browser1)
      app_browser2 = b;
  }
  ASSERT_TRUE(app_browser2);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser2, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser2->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppProtocolHandlingTest,
                       WebAppLaunch_WebAppIsLaunchedWithAllowedProtocol) {
  if (!AreProtocolHandlersSupported())
    return;

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "WebAppProtocolHandlerIntentPickerView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check that we did not add this protocol to web app's
  // allowed_launch_protocols on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_FALSE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));

  // Check the first app window is created.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser1;
  app_browser1 = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser1);

  {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "WebAppProtocolHandlerIntentPickerView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  Browser* app_browser2;
  // There should be 3 browser windows opened at the moment.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser() && b != app_browser1)
      app_browser2 = b;
  }
  ASSERT_TRUE(app_browser2);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser2, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser2->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithDiallowedProtocolUrlPref) {
  if (!AreProtocolHandlersSupported())
    return;

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(true);
  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
  base::RunLoop().RunUntilIdle();

  web_app::WebAppProtocolHandlerIntentPickerView::
      SetDefaultRememberSelectionForTesting(false);

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_TRUE(registrar.IsDisallowedLaunchProtocol(app_id, "web+test"));

  // Check the no app window is created.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithDisallowedOnceProtocol) {
  if (!AreProtocolHandlersSupported())
    return;

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "WebAppProtocolHandlerIntentPickerView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and cancels it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }

  // Check that we did not add this protocol to web app's
  // allowed_launch_protocols on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_FALSE(registrar.IsDisallowedLaunchProtocol(app_id, "web+test"));

  // Check the no app window is created.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "WebAppProtocolHandlerIntentPickerView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }
  // There should be only 1 browser window opened at the moment.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

class StartupBrowserWebAppProtocolAndFileHandlingTest
    : public StartupBrowserWebAppProtocolHandlingTest {
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
  base::test::ScopedFeatureList feature_list2_{
      features::kDesktopPWAsFileHandlingSettingsGated};
};

// Verifies that a "file://" URL on the command line is treated as a file
// handling launch, not a protocol handling or URL launch.
IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppProtocolAndFileHandlingTest,
                       WebAppLaunch_FileProtocol) {
  if (!AreProtocolHandlersSupported())
    return;

  // Install an app with protocol handlers and a handler for plain text files.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/protocol=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  apps::FileHandler file_handler;
  file_handler.action = GURL(std::string(kStartUrl) + "/file_handler");
  file_handler.accept.push_back({});
  file_handler.accept.back().mime_type = "text/plain";
  file_handler.accept.back().file_extensions = {".txt"};
  web_app::AppId app_id =
      InstallWebAppWithProtocolHandlers({protocol_handler}, {file_handler});

  // Skip the file handler dialog by simulating prior user approval of the API.
  {
    web_app::ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->UpdateApp(app_id)->SetFileHandlerApprovalState(
        web_app::ApiApprovalState::kAllowed);
  }

  // Pass a file:// url on the command line.
  SetUpCommandlineAndStart("file:///C:/test.txt", app_id);

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check an app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched to the file handler URL and not the protocol URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(file_handler.action, web_contents->GetVisibleURL());

  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
}

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)

// These tests are not applicable to Chrome OS as neither initial preferences
// nor the onboarding promos exist there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorFirstRunTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
  }
  StartupBrowserCreatorFirstRunTest(const StartupBrowserCreatorFirstRunTest&) =
      delete;
  StartupBrowserCreatorFirstRunTest& operator=(
      const StartupBrowserCreatorFirstRunTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(policy::key::kMetricsReportingEnabled,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                              /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, AddFirstRunTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title2.html"));

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), false, nullptr));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();

  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MAC)
// http://crbug.com/314819
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    DISABLED_RestoreOnStartupURLsPolicySpecified
#else
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    RestoreOnStartupURLsPolicySpecified
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_RestoreOnStartupURLsPolicySpecified) {
  if (IsWindows10OrNewer())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;

  DisableWelcomePages({browser()->profile()});

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(SessionStartupPref::kPrefValueURLs), nullptr);
  base::ListValue startup_urls;
  startup_urls.Append(embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, startup_urls.Clone(), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), true, nullptr));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the URL specified through policy is shown and no sync promo has
  // been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MAC)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsWithRestoreSession \
    DISABLED_FirstRunTabsWithRestoreSession
#else
#define MAYBE_FirstRunTabsWithRestoreSession FirstRunTabsWithRestoreSession
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsWithRestoreSession) {
  // Simulate the following initial preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html"
  //  ],
  //  "session" : {
  //    "restore_on_startup" : 1
  //   },
  //   "sync_promo" : {
  //     "user_skipped" : true
  //   }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup, 1);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), true, nullptr));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and no other pages are present.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, WelcomePages) {
  ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Welcome page should not be shown on Lacros.
  // (about:blank or new tab page will be shown instead)
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_NE(chrome::kChromeUIWelcomeURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());
#endif

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  std::unique_ptr<Profile> profile1;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }
  Profile* profile1_ptr = profile1.get();
  ASSERT_TRUE(profile1_ptr);
  profile_manager->RegisterTestingProfile(std::move(profile1), true);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip1 = browser->tab_strip_model();

  // Ensure that the standard Welcome page appears on second run on Win 10, and
  // on first run on all other platforms.
  ASSERT_EQ(1, tab_strip1->count());
  bool should_show_welcome = true;
  std::string new_tab_url1 =
      tab_strip1->GetWebContentsAt(0)->GetURL().possibly_invalid_spec();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros Welcome page appears in the new (non-main) profile only if
  // account consistency is not enabled.
  should_show_welcome =
      !base::FeatureList::IsEnabled(kMultiProfileAccountConsistency);
#endif
  if (should_show_welcome)
    EXPECT_EQ(chrome::kChromeUIWelcomeURL, new_tab_url1);
  else
    EXPECT_NE(chrome::kChromeUIWelcomeURL, new_tab_url1);

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1_ptr, ProfileKeepAliveOrigin::kBrowserWindow);

  browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
  ASSERT_TRUE(browser);
  tab_strip1 = browser->tab_strip_model();

  // Ensure that the new tab page appears on subsequent runs.
  ASSERT_EQ(1, tab_strip1->count());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip1->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       WelcomePagesWithPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_CLOUD, base::Value(4), nullptr);
  base::Value url_list(base::Value::Type::LIST);
  url_list.Append(embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_CLOUD, std::move(url_list), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  std::unique_ptr<Profile> profile1;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }
  Profile* profile1_ptr = profile1.get();
  ASSERT_TRUE(profile1_ptr);
  profile_manager->RegisterTestingProfile(std::move(profile1), true);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1_ptr, ProfileKeepAliveOrigin::kBrowserWindow);

  // Windows 10 has its own Welcome page but even that should not show up when
  // the policy is set.
  if (IsWindows10OrNewer()) {
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());

    browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
    ASSERT_TRUE(browser);
    tab_strip = browser->tab_strip_model();
  }

  // Ensure that the policy page page appears on second run on Win 10, and
  // on first run on all other platforms.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());

  browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
  ASSERT_TRUE(browser);
  tab_strip = browser->tab_strip_model();

  // Ensure that the policy page page appears on subsequent runs.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Validates that prefs::kWasRestarted is automatically reset after next browser
// start.
class StartupBrowserCreatorWasRestartedFlag : public InProcessBrowserTest,
                                              public BrowserListObserver {
 public:
  StartupBrowserCreatorWasRestartedFlag() { BrowserList::AddObserver(this); }
  ~StartupBrowserCreatorWasRestartedFlag() override {
    BrowserList::RemoveObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kUserDataDir, temp_dir_.GetPath());
    std::string json;
    base::DictionaryValue local_state;
    local_state.SetBoolean(prefs::kWasRestarted, true);
    base::JSONWriter::Write(local_state, &json);
    ASSERT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(chrome::kLocalStateFilename), json));
  }

 protected:
  // SetUpCommandLine is setting kWasRestarted, so these tests all start up
  // with WasRestarted() true.
  void OnBrowserAdded(Browser* browser) override {
    EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
    EXPECT_FALSE(
        g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
    on_browser_added_hit_ = true;
  }

  bool on_browser_added_hit_ = false;

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWasRestartedFlag, Test) {
  // OnBrowserAdded() should have been hit before the test body began.
  EXPECT_TRUE(on_browser_added_hit_);
  // This is a bit strange but what occurs is that StartupBrowserCreator runs
  // before this test body is hit and ~StartupBrowserCreator() will reset the
  // restarted state, so here when we read WasRestarted() it should already be
  // reset to false.
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
}

// The kCommandLineFlagSecurityWarningsEnabled policy doesn't exist on ChromeOS.
#if !defined(OS_CHROMEOS)
enum class CommandLineFlagSecurityWarningsPolicy {
  kNoPolicy,
  kEnabled,
  kDisabled,
};

// Verifies that infobars are displayed (or not) depending on enterprise policy.
class StartupBrowserCreatorInfobarsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          CommandLineFlagSecurityWarningsPolicy> {
 public:
  StartupBrowserCreatorInfobarsTest() : policy_(GetParam()) {}

 protected:
  infobars::ContentInfoBarManager* LaunchBrowserAndGetCreatedInfoBarManager(
      const base::CommandLine& command_line) {
    Profile* profile = browser()->profile();
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IS_NOT_FIRST_RUN);
    EXPECT_TRUE(launch.Launch(profile, true, nullptr));

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);

    return infobars::ContentInfoBarManager::FromWebContents(
        new_browser->tab_strip_model()->GetWebContentsAt(0));
  }

  const CommandLineFlagSecurityWarningsPolicy policy_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    if (policy_ != CommandLineFlagSecurityWarningsPolicy::kNoPolicy) {
      bool is_enabled =
          policy_ == CommandLineFlagSecurityWarningsPolicy::kEnabled;
      policy::PolicyMap policies;
      policies.Set(policy::key::kCommandLineFlagSecurityWarningsEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_PLATFORM, base::Value(is_enabled),
                   nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    }
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarForEnableAutomation) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableAutomation);
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  ASSERT_TRUE(infobar_manager);

  bool found_automation_infobar = false;
  for (size_t i = 0; i < infobar_manager->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE) {
      found_automation_infobar = true;
    }
  }

  EXPECT_EQ(found_automation_infobar,
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarForBadFlag) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // Test one of the flags from |bad_flags_prompt.cc|. Any of the flags should
  // have the same behavior.
  command_line.AppendSwitch(switches::kDisableWebSecurity);
  // BadFlagsPrompt::ShowBadFlagsPrompt uses CommandLine::ForCurrentProcess
  // instead of the command-line passed to StartupBrowserCreator. In browser
  // tests, this references the browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  ASSERT_TRUE(infobar_manager);

  bool found_bad_flags_infobar = false;
  for (size_t i = 0; i < infobar_manager->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE) {
      found_bad_flags_infobar = true;
    }
  }

  EXPECT_EQ(found_bad_flags_infobar,
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    PolicyControl,
    StartupBrowserCreatorInfobarsTest,
    ::testing::Values(CommandLineFlagSecurityWarningsPolicy::kNoPolicy,
                      CommandLineFlagSecurityWarningsPolicy::kEnabled,
                      CommandLineFlagSecurityWarningsPolicy::kDisabled));

#endif  // !defined(OS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that infobars are not displayed in Kiosk mode.
class StartupBrowserCreatorInfobarsKioskTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorInfobarsKioskTest() = default;

 protected:
  infobars::ContentInfoBarManager*
  LaunchKioskBrowserAndGetCreatedInfoBarManager(
      const std::string& extra_switch) {
    Profile* profile = browser()->profile();
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kKioskMode);
    command_line.AppendSwitch(extra_switch);
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IS_NOT_FIRST_RUN);
    EXPECT_TRUE(launch.Launch(profile, true, nullptr));

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);
    if (!new_browser)
      return nullptr;

    return infobars::ContentInfoBarManager::FromWebContents(
        new_browser->tab_strip_model()->GetActiveWebContents());
  }
};

// Verify that the Automation Enabled infobar is still shown in Kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForEnableAutomation) {
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kEnableAutomation);
  ASSERT_TRUE(infobar_manager);

  bool found_automation_infobar = false;
  for (size_t i = 0; i < infobar_manager->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE) {
      found_automation_infobar = true;
    }
  }

  EXPECT_TRUE(found_automation_infobar);
}

// Verify that the Bad Flags infobar is not shown in kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForBadFlag) {
  // BadFlagsPrompt::ShowBadFlagsPrompt uses CommandLine::ForCurrentProcess
  // instead of the command-line passed to StartupBrowserCreator. In browser
  // tests, this references the browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  // Passing the kDisableWebSecurity argument here presently does not do
  // anything because of the aforementioned limitation.
  // https://crbug.com/1060293
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kDisableWebSecurity);
  ASSERT_TRUE(infobar_manager);

  for (size_t i = 0; i < infobar_manager->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
    EXPECT_NE(infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
              infobar->delegate()->GetIdentifier());
  }
}

// Checks the correct behavior of the profile picker on startup.
class StartupBrowserCreatorPickerTestBase : public IdentityPlatformBrowserTest {
 public:
  StartupBrowserCreatorPickerTestBase() {
    // This test configures command line params carefully. Make sure
    // InProcessBrowserTest does _not_ add about:blank as a startup URL to the
    // command line.
    set_open_about_blank_on_browser_launch(false);
  }
  StartupBrowserCreatorPickerTestBase(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  StartupBrowserCreatorPickerTestBase& operator=(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  ~StartupBrowserCreatorPickerTestBase() override = default;

  void CreateMultipleProfiles() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // Create two additional profiles because the main test profile is created
    // later in the startup process and so we need to have at least 2 fake
    // profiles.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::vector<base::FilePath> profile_paths = {
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 1")),
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 2"))};
    for (int i = 0; i < 2; ++i) {
      const base::FilePath& profile_path = profile_paths[i];
      ASSERT_TRUE(profile_manager->GetProfile(profile_path));
      // Mark newly created profiles as active.
      ProfileAttributesEntry* entry =
          profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path);
      ASSERT_NE(entry, nullptr);
      entry->SetActiveTimeToNow();
      entry->SetAuthInfo(
          base::StringPrintf("gaia_id_%i", i),
          base::UTF8ToUTF16(base::StringPrintf("user%i@gmail.com", i)),
          /*is_consented_primary_account=*/false);
    }
  }
};

struct ProfilePickerSetup {
  enum class ShutdownType {
    kNormal,  // Normal shutdown (e.g. by closing the browser window).
    kExit,    // Exit through the application menu.
    kRestart  // Restart (e.g. after an update).
  };

  bool expected_to_show;
  absl::optional<std::string> switch_name;
  absl::optional<std::string> switch_value_ascii;
  absl::optional<GURL> url_arg;
  ShutdownType shutdown_type = ShutdownType::kNormal;
};

// Checks the correct behavior of the profile picker on startup. This feature is
// not available on ChromeOS.
class StartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<ProfilePickerSetup> {
 public:
  StartupBrowserCreatorPickerTest()
      : relaunch_chrome_override_(base::BindRepeating(
            [](const base::CommandLine&) { return true; })) {}
  StartupBrowserCreatorPickerTest(const StartupBrowserCreatorPickerTest&) =
      delete;
  StartupBrowserCreatorPickerTest& operator=(
      const StartupBrowserCreatorPickerTest&) = delete;
  ~StartupBrowserCreatorPickerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().url_arg) {
      command_line->AppendArg(GetParam().url_arg->spec());
    } else if (GetParam().switch_value_ascii) {
      DCHECK(GetParam().switch_name);
      command_line->AppendSwitchASCII(*GetParam().switch_name,
                                      *GetParam().switch_value_ascii);
    } else if (GetParam().switch_name) {
      command_line->AppendSwitch(*GetParam().switch_name);
    }
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, PRE_TestSetup) {
  CreateMultipleProfiles();

  switch (GetParam().shutdown_type) {
    case ProfilePickerSetup::ShutdownType::kNormal:
      // Need to close the browser window manually so that the real test does
      // not treat it as session restore.
      CloseAllBrowsers();
      break;
    case ProfilePickerSetup::ShutdownType::kExit:
      chrome::AttemptExit();
      break;
    case ProfilePickerSetup::ShutdownType::kRestart:
      chrome::AttemptRestart();
      break;
  }

  ASSERT_EQ(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted),
      GetParam().shutdown_type == ProfilePickerSetup::ShutdownType::kRestart);
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, TestSetup) {
  if (GetParam().expected_to_show) {
    // Opens the picker and thus does not open any new browser window for the
    // main profile.
    EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  } else {
    // The picker is skipped which means a browser window is opened on startup.
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerTest,
    ::testing::Values(
// Flaky: https://crbug.com/1126886
#if !defined(USE_OZONE) && !defined(OS_WIN)
        // Picker should be shown in normal multi-profile startup situation.
        ProfilePickerSetup{/*expected_to_show=*/true},
#endif
        // Skip the picker for various command-line params and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kIncognito},
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kApp},
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kAppId},
        // Skip the picker when a specific profile is requested (used e.g. by
        // profile specific desktop shortcuts on Win).
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileDirectory,
                           /*switch_value_ascii=*/"Default"},
        // Skip the picker when a specific profile is requested by email.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileEmail,
                           /*switch_value_ascii=*/"user0@gmail.com"},
        // Show the picker if the profile email is not found.
        ProfilePickerSetup{/*expected_to_show=*/true,
                           /*switch_name=*/switches::kProfileEmail,
                           /*switch_value_ascii=*/"unknown@gmail.com"},
        // Skip the picker when a URL is provided on command-line (used by the
        // OS when Chrome is the default web browser) and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/absl::nullopt,
                           /*switch_value_ascii=*/absl::nullopt,
                           /*url_arg=*/GURL("https://www.foo.com/")},
        // Regression test for http://crbug.com/1166192
        // Picker should be shown after exit.
        ProfilePickerSetup{
            /*expected_to_show=*/true,
            /*switch_name=*/absl::nullopt,
            /*switch_value_ascii=*/absl::nullopt,
            /*url_arg=*/absl::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kExit},
        // Regression test for http://crbug.com/1245374
        // Picker should not be shown after restart.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/absl::nullopt,
            /*switch_value_ascii=*/absl::nullopt,
            /*url_arg=*/absl::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kRestart}));

class GuestStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  GuestStartupBrowserCreatorPickerTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuest);
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithGuest) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       SkipsPickerWithGuest) {
  // The picker is skipped which means a browser window is opened on startup.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(browser()->profile()->IsGuestSession());
}

class StartupBrowserCreatorPickerNoParamsTest
    : public StartupBrowserCreatorPickerTestBase {};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       PRE_ShowPickerWhenAlreadyLaunched) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       ShowPickerWhenAlreadyLaunched) {
  // Preprequisite: The picker is shown on the first start-up
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());

  // Simulate a second start when the browser is already running.
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_dir, command_line,
                            /*ignore_profile_picker=*/false);
  EXPECT_EQ(startup_profile_path_info.mode, StartupProfileMode::kProfilePicker);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_dir, startup_profile_path_info.path);
  base::RunLoop().RunUntilIdle();

  // The picker is shown again if no profile was previously opened.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
