// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/base_tracing.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

ui::WindowShowState DetermineWindowShowState() {
  if (chrome::IsRunningInForcedAppMode())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_DEFAULT;
}

Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           Browser* target_browser) {
  DCHECK(target_browser->is_type_app());
  Browser* source_browser = chrome::FindBrowserWithWebContents(contents);

  // In a reparent, the owning session service needs to be told it's tab
  // has been removed, otherwise it will reopen the tab on restoration.
  SessionServiceBase* service =
      GetAppropriateSessionServiceForProfile(source_browser);
  service->TabClosing(contents);

  TabStripModel* source_tabstrip = source_browser->tab_strip_model();
  // Avoid causing the existing browser window to close if this is the last tab
  // remaining.
  if (source_tabstrip->count() == 1)
    chrome::NewTab(source_browser);
  target_browser->tab_strip_model()->AppendWebContents(
      source_tabstrip->DetachWebContentsAtForInsertion(
          source_tabstrip->GetIndexOfWebContents(contents)),
      true);
  target_browser->window()->Show();

  // The app window will be registered correctly, however the tab will not
  // be correctly tracked. We need to do a reset to get the tab correctly
  // tracked by the app service.
  AppSessionService* app_service =
      AppSessionServiceFactory::GetForProfile(target_browser->profile());
  app_service->ResetFromCurrentBrowsers();

  return target_browser;
}

}  // namespace

absl::optional<AppId> GetWebAppForActiveTab(Browser* browser) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(browser->profile());
  if (!provider)
    return absl::nullopt;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return absl::nullopt;

  return provider->registrar().FindInstalledAppWithUrlInScope(
      web_contents->GetMainFrame()->GetLastCommittedURL());
}

bool IsInScope(const GURL& url, const GURL& scope) {
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents) {
  content::NavigationController& navigation_controller =
      contents->GetController();
  if (!navigation_controller.CanPruneAllButLastCommitted())
    return;

  int index = navigation_controller.GetEntryCount() - 1;
  while (index >= 0 &&
         IsInScope(navigation_controller.GetEntryAtIndex(index)->GetURL(),
                   scope)) {
    --index;
  }

  while (index >= 0) {
    navigation_controller.RemoveEntryAtIndex(index);
    --index;
  }
}

Browser* ReparentWebAppForActiveTab(Browser* browser) {
  absl::optional<AppId> app_id = GetWebAppForActiveTab(browser);
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
  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile)->registrar();
  if (registrar.IsInstalled(app_id)) {
    absl::optional<GURL> app_scope = registrar.GetAppScope(app_id);
    if (!app_scope)
      app_scope = registrar.GetAppStartUrl(app_id).GetWithoutFilename();

    PrunePreScopeNavigationHistory(*app_scope, contents);
  }

  auto launch_url = contents->GetLastCommittedURL();
  RecordMetrics(app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
                extensions::AppLaunchSource::kSourceReparenting, launch_url,
                contents);

  if (registrar.IsTabbedWindowModeEnabled(app_id)) {
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (AppBrowserController::IsForWebApp(browser, app_id))
        return ReparentWebContentsIntoAppBrowser(contents, browser);
    }
  }

  return ReparentWebContentsIntoAppBrowser(
      contents,
      Browser::Create(Browser::CreateParams::CreateForApp(
          GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
          gfx::Rect(), profile, true /* user_gesture */)));
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

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser) {
  std::unique_ptr<AppBrowserController> controller;
  const AppId app_id = GetAppIdFromApplicationName(browser->app_name());
  auto* const provider =
      WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (provider && provider->registrar().IsInstalled(app_id)) {
    const SystemWebAppDelegate* system_app = nullptr;
    auto system_app_type =
        GetSystemWebAppTypeForAppId(browser->profile(), app_id);
    if (system_app_type) {
      system_app =
          provider->system_web_app_manager().GetSystemApp(*system_app_type);
    }
    const bool has_tab_strip =
        (system_app && system_app->ShouldHaveTabStrip()) ||
        (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip) &&
         provider->registrar().IsTabbedWindowModeEnabled(app_id));
    controller = std::make_unique<WebAppBrowserController>(
        *provider, browser, app_id, system_app, has_tab_strip);
  } else {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(browser->profile())
            ->GetExtensionById(app_id,
                               extensions::ExtensionRegistry::EVERYTHING);
    if (extension && extension->is_hosted_app()) {
      controller =
          std::make_unique<extensions::HostedAppBrowserController>(browser);
    }
#endif
  }
  if (controller)
    controller->Init();
  return controller;
}

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    int32_t restore_id,
                                    bool omit_from_session_restore,
                                    bool can_resize,
                                    bool can_maximize,
                                    const gfx::Rect initial_bounds) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
  Browser::CreateParams browser_params =
      disposition == WindowOpenDisposition::NEW_POPUP
          ? Browser::CreateParams::CreateForAppPopup(
                app_name, /*trusted_source=*/true, initial_bounds, profile,
                /*user_gesture=*/true)
          : Browser::CreateParams::CreateForApp(
                app_name, /*trusted_source=*/true, initial_bounds, profile,
                /*user_gesture=*/true);
  browser_params.initial_show_state = DetermineWindowShowState();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  browser_params.restore_id = restore_id;
#endif
  browser_params.omit_from_session_restore = omit_from_session_restore;
  browser_params.can_resize = can_resize;
  browser_params.can_maximize = can_maximize;
  return Browser::Create(browser_params);
}

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition) {
  NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_params.disposition = disposition;
  return NavigateWebAppUsingParams(app_id, nav_params);
}

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params) {
  Browser* browser = nav_params.browser;
  const absl::optional<SystemAppType> capturing_system_app_type =
      GetCapturingSystemAppForURL(browser->profile(), nav_params.url);
  // TODO(crbug.com/1201820): This block creates conditions where Navigate()
  // returns early and causes a crash. Fail gracefully instead. Further
  // debugging state will be implemented via Chrometto UMA traces.
  if (capturing_system_app_type &&
      (!browser ||
       !IsBrowserForSystemWebApp(browser, capturing_system_app_type.value()))) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* user_manager = user_manager::UserManager::Get();
    bool is_kiosk = user_manager && user_manager->IsLoggedInAsAnyKioskApp();
    AppBrowserController* app_controller = browser->app_controller();
    WebAppProvider* web_app_provider =
        WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
    TRACE_EVENT_INSTANT(
        "system_apps", "BadNavigate", [&](perfetto::EventContext ctx) {
          auto* bad_navigate =
              ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                  ->set_chrome_web_app_bad_navigate();
          bad_navigate->set_is_kiosk(is_kiosk);
          bad_navigate->set_has_hosted_app_controller(!!app_controller);
          bad_navigate->set_app_name(browser->app_name());
          if (app_controller && app_controller->system_app()) {
            bad_navigate->set_system_app_type(
                static_cast<uint32_t>(app_controller->system_app()->GetType()));
          }
          bad_navigate->set_web_app_provider_registry_ready(
              web_app_provider->on_registry_ready().is_signaled());
          bad_navigate->set_system_web_app_manager_synchronized(
              web_app_provider->system_web_app_manager()
                  .on_apps_synchronized()
                  .is_signaled());
        });
    UMA_HISTOGRAM_ENUMERATION("WebApp.SystemApps.BadNavigate.Type",
                              capturing_system_app_type.value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return nullptr;
  }

  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  if (web_contents) {
    WebAppTabHelper::FromWebContents(web_contents)->SetAppId(app_id);
    SetAppPrefsForWebContents(web_contents);
  }

  return web_contents;
}

void RecordAppWindowLaunch(Profile* profile, const std::string& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return;

  DisplayMode display =
      provider->registrar().GetEffectiveDisplayModeFromManifest(app_id);
  if (display == DisplayMode::kUndefined)
    return;

  DCHECK_LT(DisplayMode::kUndefined, display);
  DCHECK_LE(display, DisplayMode::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("Launch.WebAppDisplayMode", display);
}

void RecordMetrics(const AppId& app_id,
                   apps::mojom::LaunchContainer container,
                   extensions::AppLaunchSource launch_source,
                   const GURL& launch_url,
                   content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // TODO(crbug.com/1014328): Populate WebApp metrics instead of Extensions.
  if (container == apps::mojom::LaunchContainer::kLaunchContainerTab) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType",
                              extensions::LAUNCH_TYPE_REGULAR, 100);
  } else if (container ==
             apps::mojom::LaunchContainer::kLaunchContainerWindow) {
    RecordAppWindowLaunch(profile, app_id);
  }
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchSource",
                            launch_source);
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchContainer", container);

  // Record the launch time in the site engagement service. A recent web
  // app launch will provide an engagement boost to the origin.
  site_engagement::SiteEngagementService::Get(profile)
      ->SetLastShortcutLaunchTime(web_contents, launch_url);
  WebAppProvider::GetForWebApps(profile)->sync_bridge().SetAppLastLaunchTime(
      app_id, base::Time::Now());
  // Refresh the app banner added to homescreen event. The user may have
  // cleared their browsing data since installing the app, which removes the
  // event and will potentially permit a banner to be shown for the site.
  RecordAppBanner(web_contents, launch_url);
}

}  // namespace web_app
