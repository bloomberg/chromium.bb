// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager_chromeos.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace {

base::Optional<std::string> GetSettingsAppId(Profile* profile) {
  return web_app::WebAppProvider::Get(profile)
      ->system_web_app_manager()
      .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS);
}

}  // namespace

namespace chrome {

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return base::Singleton<SettingsWindowManager>::get();
}

void SettingsWindowManager::AddObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void SettingsWindowManager::RemoveObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SettingsWindowManager::ShowChromePageForProfile(Profile* profile,
                                                     const GURL& gurl) {
  // Use the original (non off-the-record) profile for settings unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord())
    profile = profile->GetOriginalProfile();

  // Look for an existing browser window.
  Browser* browser = FindBrowserForProfile(profile);
  if (browser) {
    DCHECK(browser->profile() == profile);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    if (web_contents && web_contents->GetURL() == gurl) {
      browser->window()->Show();
      return;
    }
  }
  if (web_app::SystemWebAppManager::IsEnabled()) {
    base::Optional<std::string> settings_app_id = GetSettingsAppId(profile);
    // TODO(calamity): Queue a task to launch app after it is installed.
    if (!settings_app_id)
      return;

    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            settings_app_id.value(), extensions::ExtensionRegistry::ENABLED);
    DCHECK(extension);

    // TODO(calamity): Plumb through better launch sources from callsites.
    AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
        profile, extension, 0, extensions::SOURCE_CHROME_INTERNAL,
        display::kInvalidDisplayId);
    params.override_url = gurl;
    if (browser) {
      ShowApplicationWindow(params, gurl, browser);
      return;
    }

    browser = CreateApplicationWindow(params, gurl);
    ShowApplicationWindow(params, gurl, browser);
  } else {
    if (browser) {
      NavigateParams params(browser, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.window_action = NavigateParams::SHOW_WINDOW;
      params.user_gesture = true;
      Navigate(&params);
      return;
    }

    // No existing browser window, create one.
    NavigateParams params(profile, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.trusted_source = true;
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.user_gesture = true;
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
    Navigate(&params);
    browser = params.browser;

    // operator[] not used because SessionID has no default constructor.
    settings_session_map_.emplace(profile, SessionID::InvalidValue())
        .first->second = browser->session_id();
    DCHECK(browser->is_trusted_source());

    auto* window = browser->window()->GetNativeWindow();
    window->SetProperty(kOverrideWindowIconResourceIdKey,
                        IDR_SETTINGS_LOGO_192);
    // For Mash, this is set by BrowserFrameMash.
    if (!features::IsUsingWindowService()) {
      window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::CHROME_APP));
    }
  }

  for (SettingsWindowManagerObserver& observer : observers_)
    observer.OnNewSettingsWindow(browser);
}

Browser* SettingsWindowManager::FindBrowserForProfile(Profile* profile) {
  if (web_app::SystemWebAppManager::IsEnabled()) {
    // TODO(calamity): Determine whether, during startup, we need to wait for
    // app install and then provide a valid answer here.
    base::Optional<std::string> settings_app_id = GetSettingsAppId(profile);
    if (!settings_app_id)
      return nullptr;

    // Check if any settings windows are already running.
    std::vector<content::WebContents*> running_apps =
        AppShortcutLauncherItemController::GetRunningApplications(
            settings_app_id.value());
    if (!running_apps.empty())
      return chrome::FindBrowserWithWebContents(running_apps[0]);

    return nullptr;
  }

  auto iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);

  return nullptr;
}

bool SettingsWindowManager::IsSettingsBrowser(Browser* browser) const {
  Profile* profile = browser->profile();
  if (web_app::SystemWebAppManager::IsEnabled()) {
    // TODO(calamity): Determine whether, during startup, we need to wait for
    // app install and then provide a valid answer here.
    base::Optional<std::string> settings_app_id = GetSettingsAppId(profile);
    if (!settings_app_id)
      return false;

    return browser->hosted_app_controller() &&
           browser->hosted_app_controller()->app_id() ==
               settings_app_id.value();
  } else {
    auto iter = settings_session_map_.find(profile);
    return iter != settings_session_map_.end() &&
           iter->second == browser->session_id();
  }
}

SettingsWindowManager::SettingsWindowManager() {}

SettingsWindowManager::~SettingsWindowManager() {}

}  // namespace chrome
