// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "url/gurl.h"

namespace {

bool IsInScope(content::NavigationEntry* entry, const std::string& scope_spec) {
  return base::StartsWith(entry->GetURL().spec(), scope_spec,
                          base::CompareCase::SENSITIVE);
}

Browser* ReparentWebContentsWithBrowserCreateParams(
    content::WebContents* contents,
    const Browser::CreateParams& browser_params) {
  Browser* source_browser = chrome::FindBrowserWithWebContents(contents);
  Browser* target_browser = Browser::Create(browser_params);

  TabStripModel* source_tabstrip = source_browser->tab_strip_model();
  // Avoid causing the existing browser window to close if this is the last tab
  // remaining.
  if (source_tabstrip->count() == 1)
    chrome::NewTab(source_browser);
  target_browser->tab_strip_model()->AppendWebContents(
      source_tabstrip->DetachWebContentsAt(
          source_tabstrip->GetIndexOfWebContents(contents)),
      true);
  target_browser->window()->Show();

  return target_browser;
}

}  // namespace

namespace web_app {

base::Optional<AppId> GetWebAppForActiveTab(Browser* browser) {
  WebAppProvider* provider = WebAppProvider::Get(browser->profile());
  if (!provider)
    return base::nullopt;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::nullopt;

  return provider->registrar().FindAppWithUrlInScope(
      web_contents->GetMainFrame()->GetLastCommittedURL());
}

void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents) {
  content::NavigationController& navigation_controller =
      contents->GetController();
  if (!navigation_controller.CanPruneAllButLastCommitted())
    return;

  const std::string scope_spec = scope.spec();
  int index = navigation_controller.GetEntryCount() - 1;
  while (index >= 0 &&
         IsInScope(navigation_controller.GetEntryAtIndex(index), scope_spec)) {
    --index;
  }

  while (index >= 0) {
    navigation_controller.RemoveEntryAtIndex(index);
    --index;
  }
}

Browser* ReparentWebAppForActiveTab(Browser* browser) {
  base::Optional<AppId> app_id = GetWebAppForActiveTab(browser);
  if (!app_id)
    return nullptr;
  return ReparentWebContentsIntoAppBrowser(
      browser->tab_strip_model()->GetActiveWebContents(), *app_id);
}

Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const AppId& app_id) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // Incognito tabs reparent correctly, but remain incognito without any
  // indication to the user, so disallow it.
  DCHECK(!profile->IsOffTheRecord());

  // Clear navigation history that occurred before the user most recently
  // entered the app's scope. The minimal-ui Back button will be initially
  // disabled if the previous page was outside scope. Packaged apps are not
  // affected.
  AppRegistrar& registrar =
      WebAppProviderBase::GetProviderBase(profile)->registrar();
  if (registrar.IsInstalled(app_id)) {
    base::Optional<GURL> app_scope = registrar.GetAppScope(app_id);
    if (!app_scope)
      app_scope = registrar.GetAppLaunchURL(app_id).GetWithoutFilename();

    PrunePreScopeNavigationHistory(*app_scope, contents);
  }

  Browser::CreateParams browser_params(Browser::CreateParams::CreateForApp(
      GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
      gfx::Rect(), profile, true /* user_gesture */));
  return ReparentWebContentsWithBrowserCreateParams(contents, browser_params);
}

Browser* ReparentWebContentsForFocusMode(content::WebContents* contents) {
  DCHECK(base::FeatureList::IsEnabled(features::kFocusMode));
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // TODO(crbug.com/941577): Remove DCHECK when focus mode is permitted in guest
  // and incognito sessions.
  DCHECK(!profile->IsOffTheRecord());
  Browser::CreateParams browser_params(Browser::CreateParams::CreateForApp(
      GenerateApplicationNameForFocusMode(), true /* trusted_source */,
      gfx::Rect(), profile, true /* user_gesture */));
  browser_params.is_focus_mode = true;
  return ReparentWebContentsWithBrowserCreateParams(contents, browser_params);
}

void SetAppPrefsForWebContents(content::WebContents* web_contents) {
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  web_contents->SyncRendererPrefs();

  web_contents->NotifyPreferencesChanged();
}

void ClearAppPrefsForWebContents(content::WebContents* web_contents) {
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = true;
  web_contents->SyncRendererPrefs();

  web_contents->NotifyPreferencesChanged();
}

}  // namespace web_app
