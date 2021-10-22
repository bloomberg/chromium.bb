// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

#include <memory>

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class UnittestingSystemAppDelegate : public SystemWebAppDelegate {
 public:
  UnittestingSystemAppDelegate(SystemAppType type,
                               const std::string& name,
                               const GURL& url,
                               WebApplicationInfoFactory info_factory);
  UnittestingSystemAppDelegate(const UnittestingSystemAppDelegate&) = delete;
  UnittestingSystemAppDelegate& operator=(const UnittestingSystemAppDelegate&) =
      delete;
  ~UnittestingSystemAppDelegate() override;

  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;

  std::vector<AppId> GetAppIdsToUninstallAndReplace() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool ShouldBeSingleWindow() const override;
  bool ShouldShowNewWindowMenuOption() const override;
  bool ShouldIncludeLaunchDirectory() const override;
  std::vector<int> GetAdditionalSearchTerms() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearch() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldAllowResize() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldHaveTabStrip() const override;
  bool ShouldHaveReloadButtonInMinimalUi() const override;
  bool ShouldAllowScriptsToCloseWindows() const override;
  absl::optional<SystemAppBackgroundTaskInfo> GetTimerInfo() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
  bool IsAppEnabled() const override;

  void SetAppIdsToUninstallAndReplace(const std::vector<AppId>&);
  void SetMinimumWindowSize(const gfx::Size&);
  void SetShouldBeSingleWindow(bool);
  void SetShouldShowNewWindowMenuOption(bool);
  void SetShouldIncludeLaunchDirectory(bool);
  void SetEnabledOriginTrials(const OriginTrialsMap&);
  void SetAdditionalSearchTerms(const std::vector<int>&);
  void SetShouldShowInLauncher(bool);
  void SetShouldShowInSearch(bool);
  void SetShouldCaptureNavigations(bool);
  void SetShouldAllowResize(bool);
  void SetShouldAllowMaximize(bool);
  void SetShouldHaveTabStrip(bool);
  void SetShouldHaveReloadButtonInMinimalUi(bool);
  void SetShouldAllowScriptsToCloseWindows(bool);
  void SetTimerInfo(const SystemAppBackgroundTaskInfo&);
  void SetDefaultBounds(base::RepeatingCallback<gfx::Rect(Browser*)>);
  void SetUrlInSystemAppScope(const GURL& url);

 private:
  WebApplicationInfoFactory info_factory_;

  std::vector<AppId> uninstall_and_replace_;
  gfx::Size minimum_window_size_;
  bool single_window_ = true;
  bool show_new_window_menu_option_ = false;
  bool include_launch_directory_ = false;
  std::vector<int> additional_search_terms_;
  bool show_in_launcher_ = true;
  bool show_in_search_ = true;
  bool capture_navigations_ = false;
  bool is_resizeable_ = true;
  bool is_maximizable_ = true;
  bool has_tab_strip_ = false;
  bool should_have_reload_button_in_minimal_ui_ = true;
  bool allow_scripts_to_close_windows_ = false;

  base::RepeatingCallback<gfx::Rect(Browser*)> get_default_bounds_ =
      base::NullCallback();

  absl::optional<SystemAppBackgroundTaskInfo> timer_info_;
};

// Class to setup the installation of a test System Web App.
//
// Use SetUp*() methods to create a instance of this class in test suite's
// constructor, before the profile is fully created. In tests, call
// WaitForAppInstall() to finish the installation.
class TestSystemWebAppInstallation {
 public:
  enum IncludeLaunchDirectory { kYes, kNo };

  // Used for tests that don't want to install any System Web Apps.
  static std::unique_ptr<TestSystemWebAppInstallation> SetUpWithoutApps();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpTabbedMultiWindowApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpStandaloneSingleWindowApp();

  // This method automatically grants File System Access read and write
  // permissions to the App.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatReceivesLaunchFiles(
      IncludeLaunchDirectory include_launch_directory);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithEnabledOriginTrials(const OriginTrialsMap& origin_to_trials);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInLauncher();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInSearch();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithAdditionalSearchTerms();

  // This method additionally sets up a helper SystemAppType::SETTING system app
  // for testing capturing links from a different SWA.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatCapturesNavigation();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpChromeUntrustedApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpNonResizeableAndNonMaximizableApp();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithBackgroundTask();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetupAppWithAllowScriptsToCloseWindows(bool value);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithTabStrip(
      bool has_tab_strip);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithDefaultBounds(const gfx::Rect& default_bounds);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithNewWindowMenuItem();

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppWithShortcuts();

  // This creates 4 system web app types for testing context menu with
  // different windowing options:
  //
  // - SETTINGS: Single Window, No TabStrip
  // - FILE_MANAGER: Multi Window, No TabStrip
  // - MEDIA: Single Window, TabStrip
  // - HELP: Multi Window, TabStrip
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppsForContestMenuTest();

  ~TestSystemWebAppInstallation();

  void WaitForAppInstall();

  AppId GetAppId();
  const GURL& GetAppUrl();
  SystemAppType GetType();

  void set_update_policy(SystemWebAppManager::UpdatePolicy update_policy) {
    update_policy_ = update_policy;
  }

 private:
  TestSystemWebAppInstallation(
      std::unique_ptr<UnittestingSystemAppDelegate> system_app_delegate);
  TestSystemWebAppInstallation();

  std::unique_ptr<KeyedService> CreateWebAppProvider(
      UnittestingSystemAppDelegate* system_app_delegate,
      Profile* profile);
  std::unique_ptr<KeyedService> CreateWebAppProviderWithNoSystemWebApps(
      Profile* profile);

  // Must be called in SetUp*App() methods, before WebAppProvider is created.
  void RegisterAutoGrantedPermissions(ContentSettingsType permission);

  Profile* profile_;
  SystemWebAppManager::UpdatePolicy update_policy_ =
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate;
  std::unique_ptr<FakeWebAppProviderCreator> fake_web_app_provider_creator_;
  // nullopt if SetUpWithoutApps() was used.
  const absl::optional<SystemAppType> type_;
  std::vector<std::unique_ptr<TestSystemWebAppWebUIControllerFactory>>
      web_ui_controller_factories_;
  std::set<ContentSettingsType> auto_granted_permissions_;
  base::flat_map<SystemAppType, std::unique_ptr<SystemWebAppDelegate>>
      system_app_delegates_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
