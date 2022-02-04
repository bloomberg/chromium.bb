// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_url_data_source.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace web_app {

namespace {

enum class WebUIType {
  // kChrome WebUIs registration works by creating a WebUIControllerFactory
  // which then register a URLDataSource to serve resources.
  kChrome,
  // kChromeUntrusted WebUIs don't have WebUIControllers and their
  // URLDataSources need to be registered directly.
  kChromeUntrusted,
};

WebUIType GetWebUIType(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return WebUIType::kChrome;
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return WebUIType::kChromeUntrusted;
  NOTREACHED();
  return WebUIType::kChrome;
}

// Assumes url is like "chrome://web-app/index.html". Returns "web-app";
// This function is needed because at the time TestSystemWebInstallation is
// initialized, chrome scheme is not yet registered with GURL, so it will be
// parsed as PathURL, resulting in an empty host.
std::string GetDataSourceNameFromSystemAppInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIScheme);

  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  return spec.substr(p, pos_after_host - p);
}

// Returns the scheme and host from an install URL e.g. for
// chrome-untrusted://web-app/index.html this returns
// chrome-untrusted://web-app/.
std::string GetChromeUntrustedDataSourceNameFromInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIUntrustedScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIUntrustedScheme);
  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  // The Data Source name must include "/" after the host.
  ++pos_after_host;
  return spec.substr(0, pos_after_host);
}

}  // namespace

UnittestingSystemAppDelegate::UnittestingSystemAppDelegate(
    SystemAppType type,
    const std::string& name,
    const GURL& url,
    WebAppInstallInfoFactory info_factory)
    : web_app::SystemWebAppDelegate(type, name, url, nullptr),
      info_factory_(info_factory) {}

UnittestingSystemAppDelegate::~UnittestingSystemAppDelegate() = default;

std::unique_ptr<WebAppInstallInfo> UnittestingSystemAppDelegate::GetWebAppInfo()
    const {
  return info_factory_.Run();
}

std::vector<AppId>
UnittestingSystemAppDelegate::GetAppIdsToUninstallAndReplace() const {
  return uninstall_and_replace_;
}

gfx::Size UnittestingSystemAppDelegate::GetMinimumWindowSize() const {
  return minimum_window_size_;
}
bool UnittestingSystemAppDelegate::ShouldReuseExistingWindow() const {
  return single_window_;
}
bool UnittestingSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return show_new_window_menu_option_;
}
base::FilePath UnittestingSystemAppDelegate::GetLaunchDirectory(
    const apps::AppLaunchParams& params) const {
  // When set to include a launch directory, use the directory of the first
  // file.
  return include_launch_directory_ && !params.launch_files.empty()
             ? params.launch_files[0].DirName()
             : base::FilePath();
}

std::vector<int> UnittestingSystemAppDelegate::GetAdditionalSearchTerms()
    const {
  return additional_search_terms_;
}
bool UnittestingSystemAppDelegate::ShouldShowInLauncher() const {
  return show_in_launcher_;
}
bool UnittestingSystemAppDelegate::ShouldShowInSearch() const {
  return show_in_search_;
}
bool UnittestingSystemAppDelegate::ShouldHandleFileOpenIntents() const {
  return handles_file_open_intents_;
}
bool UnittestingSystemAppDelegate::ShouldCaptureNavigations() const {
  return capture_navigations_;
}
bool UnittestingSystemAppDelegate::ShouldAllowResize() const {
  return is_resizeable_;
}
bool UnittestingSystemAppDelegate::ShouldAllowMaximize() const {
  return is_maximizable_;
}
bool UnittestingSystemAppDelegate::ShouldHaveTabStrip() const {
  return has_tab_strip_;
}
bool UnittestingSystemAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return should_have_reload_button_in_minimal_ui_;
}
bool UnittestingSystemAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return allow_scripts_to_close_windows_;
}
absl::optional<SystemAppBackgroundTaskInfo>
UnittestingSystemAppDelegate::GetTimerInfo() const {
  return timer_info_;
}
gfx::Rect UnittestingSystemAppDelegate::GetDefaultBounds(
    Browser* browser) const {
  if (get_default_bounds_) {
    return get_default_bounds_.Run(browser);
  }
  return gfx::Rect();
}
bool UnittestingSystemAppDelegate::IsAppEnabled() const {
  return true;
}
bool UnittestingSystemAppDelegate::IsUrlInSystemAppScope(
    const GURL& url) const {
  return url == url_in_system_app_scope_;
}

void UnittestingSystemAppDelegate::SetAppIdsToUninstallAndReplace(
    const std::vector<AppId>& ids) {
  uninstall_and_replace_ = ids;
}
void UnittestingSystemAppDelegate::SetMinimumWindowSize(const gfx::Size& size) {
  minimum_window_size_ = size;
}
void UnittestingSystemAppDelegate::SetShouldReuseExistingWindow(bool value) {
  single_window_ = value;
}
void UnittestingSystemAppDelegate::SetShouldShowNewWindowMenuOption(
    bool value) {
  show_new_window_menu_option_ = value;
}
void UnittestingSystemAppDelegate::SetShouldIncludeLaunchDirectory(bool value) {
  include_launch_directory_ = value;
}
void UnittestingSystemAppDelegate::SetEnabledOriginTrials(
    const OriginTrialsMap& map) {
  origin_trials_map_ = map;
}
void UnittestingSystemAppDelegate::SetAdditionalSearchTerms(
    const std::vector<int>& terms) {
  additional_search_terms_ = terms;
}
void UnittestingSystemAppDelegate::SetShouldShowInLauncher(bool value) {
  show_in_launcher_ = value;
}
void UnittestingSystemAppDelegate::SetShouldShowInSearch(bool value) {
  show_in_search_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHandleFileOpenIntents(bool value) {
  handles_file_open_intents_ = value;
}
void UnittestingSystemAppDelegate::SetShouldCaptureNavigations(bool value) {
  capture_navigations_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowResize(bool value) {
  is_resizeable_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowMaximize(bool value) {
  is_maximizable_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHaveTabStrip(bool value) {
  has_tab_strip_ = value;
}
void UnittestingSystemAppDelegate::SetShouldHaveReloadButtonInMinimalUi(
    bool value) {
  should_have_reload_button_in_minimal_ui_ = value;
}
void UnittestingSystemAppDelegate::SetShouldAllowScriptsToCloseWindows(
    bool value) {
  allow_scripts_to_close_windows_ = value;
}
void UnittestingSystemAppDelegate::SetTimerInfo(
    const SystemAppBackgroundTaskInfo& timer_info) {
  timer_info_ = timer_info;
}
void UnittestingSystemAppDelegate::SetDefaultBounds(
    base::RepeatingCallback<gfx::Rect(Browser*)> lambda) {
  get_default_bounds_ = std::move(lambda);
}
void UnittestingSystemAppDelegate::SetUrlInSystemAppScope(const GURL& url) {
  url_in_system_app_scope_ = url;
}

TestSystemWebAppInstallation::TestSystemWebAppInstallation(
    std::unique_ptr<UnittestingSystemAppDelegate> delegate)
    : type_(delegate->GetType()) {
  if (GetWebUIType(delegate->GetInstallUrl()) == WebUIType::kChrome) {
    auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
        GetDataSourceNameFromSystemAppInstallUrl(delegate->GetInstallUrl()));
    web_ui_controller_factories_.push_back(std::move(factory));
  }

  UnittestingSystemAppDelegate* delegate_ptr = delegate.get();
  system_app_delegates_.emplace(type_.value(), std::move(delegate));

  fake_web_app_provider_creator_ = std::make_unique<FakeWebAppProviderCreator>(
      base::BindRepeating(&TestSystemWebAppInstallation::CreateWebAppProvider,
                          // base::Unretained is safe here. This callback is
                          // called at TestingProfile::Init, which is at test
                          // startup. TestSystemWebAppInstallation is intended
                          // to have the same lifecycle as the test, it won't be
                          // destroyed before the test finishes.
                          base::Unretained(this), delegate_ptr));
}

TestSystemWebAppInstallation::TestSystemWebAppInstallation() {
  fake_web_app_provider_creator_ = std::make_unique<
      FakeWebAppProviderCreator>(base::BindRepeating(
      &TestSystemWebAppInstallation::CreateWebAppProviderWithNoSystemWebApps,
      // base::Unretained is safe here. This callback is called
      // at TestingProfile::Init, which is at test startup.
      // TestSystemWebAppInstallation is intended to have the
      // same lifecycle as the test, it won't be destroyed before
      // the test finishes.
      base::Unretained(this)));
}

TestSystemWebAppInstallation::~TestSystemWebAppInstallation() = default;

std::unique_ptr<WebAppInstallInfo> GenerateWebAppInstallInfoForTestApp() {
  auto info = std::make_unique<WebAppInstallInfo>();
  // the pwa.html is arguably wrong, but the manifest version uses it
  // incorrectly as well, and it's a lot of work to fix it. App ids are
  // generated from this, and it's important to keep it stable across the
  // installation modes.
  info->start_url = GURL("chrome://test-system-app/pwa.html");
  info->scope = GURL("chrome://test-system-app/");
  info->title = u"Test System App";
  info->theme_color = 0xFF00FF00;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = DisplayMode::kStandalone;
  return info;
}

std::unique_ptr<WebAppInstallInfo>
GenerateWebAppInstallInfoForTestAppUntrusted() {
  auto info = GenerateWebAppInstallInfoForTestApp();
  info->start_url = GURL("chrome-untrusted://test-system-app/pwa.html");
  info->scope = GURL("chrome-untrusted://test-system-app/");
  return info;
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpWithoutApps() {
  return base::WrapUnique(new TestSystemWebAppInstallation());
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::TERMINAL, "Terminal",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldReuseExistingWindow(false);
  delegate->SetShouldHaveTabStrip(true);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::SETTINGS, "OSSettings",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetUrlInSystemAppScope(GURL("http://example.com/in-scope"));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
    IncludeLaunchDirectory include_launch_directory) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Media",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  if (include_launch_directory == IncludeLaunchDirectory::kYes) {
    delegate->SetShouldIncludeLaunchDirectory(true);
  }

  auto* installation = new TestSystemWebAppInstallation(std::move(delegate));
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
    const OriginTrialsMap& origin_to_trials) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Media",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  delegate->SetEnabledOriginTrials(origin_to_trials);
  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInLauncher() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  delegate->SetShouldShowInLauncher(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInSearch() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldShowInSearch(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatHandlesFileOpenIntents() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldHandleFileOpenIntents(true);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetAdditionalSearchTerms({IDS_SETTINGS_SECURITY});

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::HELP, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldCaptureNavigations(true);

  auto* installation = new TestSystemWebAppInstallation(std::move(delegate));

  // Add a helper system app to test capturing links from it.
  const GURL kInitiatingAppUrl = GURL("chrome://initiating-app/pwa.html");
  installation->system_app_delegates_.insert_or_assign(
      SystemAppType::SETTINGS,
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::SETTINGS, "Initiating App", kInitiatingAppUrl,
          base::BindLambdaForTesting([]() {
            auto info = std::make_unique<WebAppInstallInfo>();
            // the pwa.html is arguably wrong, but the manifest
            // version uses it incorrectly as well, and it's a lot of
            // work to fix it. App ids are generated from this, and
            // it's important to keep it stable across the
            // installation modes.
            info->start_url = GURL("chrome://initiating-app/pwa.html");
            info->scope = GURL("chrome://initiating-app/");
            info->title = u"Test System App";
            info->theme_color = 0xFF00FF00;
            info->display_mode = blink::mojom::DisplayMode::kStandalone;
            info->user_display_mode = DisplayMode::kStandalone;
            return info;
          })));
  auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
      kInitiatingAppUrl.host());
  installation->web_ui_controller_factories_.push_back(std::move(factory));

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpChromeUntrustedApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::SETTINGS, "Test",
          GURL("chrome-untrusted://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestAppUntrusted))));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpNonResizeableAndNonMaximizableApp() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldAllowResize(false);
  delegate->SetShouldAllowMaximize(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithBackgroundTask() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));

  SystemAppBackgroundTaskInfo background_task(
      base::Days(1), GURL("chrome://test-system-app/page2.html"), true);
  delegate->SetTimerInfo(background_task);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetupAppWithAllowScriptsToCloseWindows(
    bool value) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  /* The default value of allow_scripts_to_close_windows is false. */
  if (value) {
    delegate->SetShouldAllowScriptsToCloseWindows(value);
  }
  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithTabStrip(bool has_tab_strip) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldHaveTabStrip(has_tab_strip);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithDefaultBounds(
    const gfx::Rect& default_bounds) {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::MEDIA, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetDefaultBounds(
      base::BindLambdaForTesting([&](Browser*) { return default_bounds; }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithNewWindowMenuItem() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::FILE_MANAGER, "Test",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebAppInstallInfoForTestApp));
  delegate->SetShouldShowNewWindowMenuOption(true);
  delegate->SetShouldReuseExistingWindow(false);

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithShortcuts() {
  std::unique_ptr<UnittestingSystemAppDelegate> delegate =
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemAppType::SHORTCUT_CUSTOMIZATION, "Shortcuts",
          GURL("chrome://test-system-app/pwa.html"),
          base::BindLambdaForTesting([]() {
            std::unique_ptr<WebAppInstallInfo> info =
                GenerateWebAppInstallInfoForTestApp();
            info->title = u"Shortcuts";
            {
              WebAppShortcutsMenuItemInfo menu_item;
              menu_item.name = u"One";
              menu_item.url = GURL("chrome://test-system-app/pwa.html#one");
              info->shortcuts_menu_item_infos.push_back(std::move(menu_item));
            }
            {
              WebAppShortcutsMenuItemInfo menu_item;
              menu_item.name = u"Two";
              menu_item.url = GURL("chrome://test-system-app/pwa.html#two");
              info->shortcuts_menu_item_infos.push_back(std::move(menu_item));
            }
            return info;
          }));

  return base::WrapUnique(
      new TestSystemWebAppInstallation(std::move(delegate)));
}

namespace {
enum SystemWebAppWindowConfig {
  SINGLE_WINDOW,
  SINGLE_WINDOW_TAB_STRIP,
  MULTI_WINDOW,
  MULTI_WINDOW_TAB_STRIP,
};

std::unique_ptr<UnittestingSystemAppDelegate>
CreateSystemAppDelegateWithWindowConfig(
    const SystemAppType type,
    const GURL& app_url,
    SystemWebAppWindowConfig window_config) {
  auto* delegate = new UnittestingSystemAppDelegate(
      type, "Test App", app_url, base::BindLambdaForTesting([=]() {
        auto info = std::make_unique<WebAppInstallInfo>();
        info->start_url = app_url;
        info->scope = app_url.DeprecatedGetOriginAsURL();
        info->title = u"Test System App";
        info->theme_color = 0xFF00FF00;
        info->display_mode = blink::mojom::DisplayMode::kStandalone;
        info->user_display_mode = DisplayMode::kStandalone;
        return info;
      }));

  switch (window_config) {
    case SystemWebAppWindowConfig::SINGLE_WINDOW:
      delegate->SetShouldReuseExistingWindow(true);
      delegate->SetShouldHaveTabStrip(false);
      break;
    case SystemWebAppWindowConfig::SINGLE_WINDOW_TAB_STRIP:
      delegate->SetShouldReuseExistingWindow(true);
      delegate->SetShouldHaveTabStrip(true);
      break;
    case SystemWebAppWindowConfig::MULTI_WINDOW:
      delegate->SetShouldReuseExistingWindow(false);
      delegate->SetShouldHaveTabStrip(false);
      break;
    case SystemWebAppWindowConfig::MULTI_WINDOW_TAB_STRIP:
      delegate->SetShouldReuseExistingWindow(false);
      delegate->SetShouldHaveTabStrip(true);
      break;
  }

  return base::WrapUnique(delegate);
}

}  // namespace

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppsForContestMenuTest() {
  std::vector<std::unique_ptr<UnittestingSystemAppDelegate>> delegates;
  delegates.emplace_back(CreateSystemAppDelegateWithWindowConfig(
      SystemAppType::SETTINGS, GURL("chrome://single-window/pwa.html"),
      SystemWebAppWindowConfig::SINGLE_WINDOW));

  delegates.emplace_back(CreateSystemAppDelegateWithWindowConfig(
      SystemAppType::FILE_MANAGER, GURL("chrome://multi-window/pwa.html"),
      SystemWebAppWindowConfig::MULTI_WINDOW));

  delegates.emplace_back(CreateSystemAppDelegateWithWindowConfig(
      SystemAppType::MEDIA, GURL("chrome://single-window-tab-strip/pwa.html"),
      SystemWebAppWindowConfig::SINGLE_WINDOW_TAB_STRIP));

  delegates.emplace_back(CreateSystemAppDelegateWithWindowConfig(
      SystemAppType::HELP, GURL("chrome://multi-window-tab-strip/pwa.html"),
      SystemWebAppWindowConfig::MULTI_WINDOW_TAB_STRIP));
  auto* installation =
      new TestSystemWebAppInstallation(std::move(delegates[0]));

  for (size_t i = 1; i < delegates.size(); ++i) {
    auto& delegate = delegates[i];
    installation->web_ui_controller_factories_.push_back(
        std::make_unique<TestSystemWebAppWebUIControllerFactory>(
            GetDataSourceNameFromSystemAppInstallUrl(
                delegate->GetInstallUrl())));

    installation->system_app_delegates_.insert_or_assign(delegate->GetType(),
                                                         std::move(delegate));
  }

  return base::WrapUnique(installation);
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProvider(
    UnittestingSystemAppDelegate* delegate,
    Profile* profile) {
  profile_ = profile;
  if (GetWebUIType(delegate->GetInstallUrl()) == WebUIType::kChromeUntrusted) {
    AddTestURLDataSource(GetChromeUntrustedDataSourceNameFromInstallUrl(
                             delegate->GetInstallUrl()),
                         profile);
  }

  auto provider = std::make_unique<FakeWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);

  system_web_app_manager->SetSystemAppsForTesting(
      std::move(system_app_delegates_));
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();

  const url::Origin app_origin = url::Origin::Create(delegate->GetInstallUrl());
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile);
  for (const auto& permission : auto_granted_permissions_)
    allowlist->RegisterAutoGrantedPermission(app_origin, permission);

  return provider;
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProviderWithNoSystemWebApps(
    Profile* profile) {
  profile_ = profile;
  auto provider = std::make_unique<FakeWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);
  system_web_app_manager->SetSystemAppsForTesting({});
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();
  return provider;
}

void TestSystemWebAppInstallation::WaitForAppInstall() {
  base::RunLoop run_loop;
  WebAppProvider::GetForTest(profile_)
      ->system_web_app_manager()
      .on_apps_synchronized()
      .Post(FROM_HERE, base::BindLambdaForTesting([&]() {
              // Wait one execution loop for on_apps_synchronized() to be
              // called on all listeners.
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, run_loop.QuitClosure());
            }));
  run_loop.Run();
}

AppId TestSystemWebAppInstallation::GetAppId() {
  return WebAppProvider::GetForTest(profile_)
      ->system_web_app_manager()
      .GetAppIdForSystemApp(type_.value())
      .value();
}

const GURL& TestSystemWebAppInstallation::GetAppUrl() {
  return WebAppProvider::GetForTest(profile_)->registrar().GetAppStartUrl(
      GetAppId());
}

SystemAppType TestSystemWebAppInstallation::GetType() {
  return type_.value();
}

void TestSystemWebAppInstallation::RegisterAutoGrantedPermissions(
    ContentSettingsType permission) {
  auto_granted_permissions_.insert(permission);
}

}  // namespace web_app
