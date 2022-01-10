// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/common_apps_navigation_throttle.h"

#include <string>
#include <utility>

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/browser_resources.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/types/display_constants.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// TODO(crbug.com/1251490 ) Update to make disabled page works in Lacros.
std::string GetAppDisabledErrorPage() {
  base::DictionaryValue strings;

  strings.SetString(
      "disabledPageHeader",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER));
  strings.SetString(
      "disabledPageTitle",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_TITLE));
  strings.SetString(
      "disabledPageMessage",
      l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_MESSAGE));
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CHROME_URLS_DISABLED_PAGE_HTML);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  return webui::GetI18nTemplateHtml(html, &strings);
}

bool IsAppDisabled(const std::string& app_id) {
  policy::SystemFeature system_feature =
      policy::SystemFeaturesDisableListPolicyHandler::GetSystemFeatureFromAppId(
          app_id);

  if (system_feature == policy::SystemFeature::kUnknownSystemFeature)
    return false;

  return policy::SystemFeaturesDisableListPolicyHandler::
      IsSystemFeatureDisabled(system_feature, g_browser_process->local_state());
}

// Usually we want to only capture navigations from clicking a link. For a
// subset of apps, we want to capture typing into the omnibox as well.
bool ShouldOnlyCaptureLinks(const std::vector<std::string>& app_ids) {
  for (auto app_id : app_ids) {
    if (app_id == ash::kChromeUITrustedProjectorSwaAppId)
      return false;
  }
  return true;
}

bool IsSystemWebApp(Profile* profile, const std::string& app_id) {
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app](const apps::AppUpdate& update) {
        if (update.InstallReason() == apps::mojom::InstallReason::kSystem) {
          is_system_web_app = true;
        }
      });
  return is_system_web_app;
}

// This function redirects an external untrusted |url| to a privileged trusted
// one for SWAs, if applicable.
GURL RedirectUrlIfSwa(Profile* profile,
                      const std::string& app_id,
                      const GURL& url) {
  if (!IsSystemWebApp(profile, app_id))
    return url;

  // Projector:
  if (app_id == ash::kChromeUITrustedProjectorSwaAppId &&
      url.DeprecatedGetOriginAsURL() ==
          GURL(ash::kChromeUIUntrustedProjectorPwaUrl)
              .DeprecatedGetOriginAsURL()) {
    std::string override_url = ash::kChromeUITrustedProjectorAppUrl;
    if (url.path().length() > 1)
      override_url += url.path().substr(1);
    GURL result(override_url);
    DCHECK(result.is_valid());
    return result;
  }
  // Add redirects for other SWAs above this line.

  // No matching SWAs found, returning original url.
  return url;
}

IntentHandlingMetrics::Platform GetMetricsPlatform(mojom::AppType app_type) {
  switch (app_type) {
    case mojom::AppType::kArc:
      return IntentHandlingMetrics::Platform::ARC;
    case mojom::AppType::kWeb:
      return IntentHandlingMetrics::Platform::PWA;
    default:
      NOTREACHED();
      return IntentHandlingMetrics::Platform::ARC;
  }
}

}  // namespace

// static
std::unique_ptr<apps::AppsNavigationThrottle>
CommonAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. portals, fenced-frame). We specifically allow
  // prerendering navigations so that we can destroy the prerender. Opening an
  // app must only happen when the user intentionally navigates; however, for a
  // prerender, the prerender-activating navigation doesn't run throttles so we
  // must cancel it during initial loading to get a standard (non-prerendering)
  // navigation at link-click-time.
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return nullptr;

  if (!ShouldCheckAppsForUrl(web_contents))
    return nullptr;

  return std::make_unique<CommonAppsNavigationThrottle>(handle);
}

CommonAppsNavigationThrottle::CommonAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

CommonAppsNavigationThrottle::~CommonAppsNavigationThrottle() = default;

bool CommonAppsNavigationThrottle::ShouldCancelNavigation(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();

  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browser=*/true);

  if (app_ids.empty())
    return false;

  if (ShouldOnlyCaptureLinks(app_ids) && !navigate_from_link())
    return false;

  absl::optional<std::string> preferred_app_id =
      proxy->PreferredApps().FindPreferredAppForUrl(url);
  if (!preferred_app_id.has_value() ||
      !base::Contains(app_ids, preferred_app_id.value())) {
    return false;
  }

  // Only automatically launch PWA if the flag is on.
  apps::mojom::AppType app_type =
      proxy->AppRegistryCache().GetAppType(preferred_app_id.value());
  if (app_type != apps::mojom::AppType::kArc &&
      (app_type != apps::mojom::AppType::kWeb ||
       !base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence)) &&
      !IsSystemWebApp(profile, preferred_app_id.value())) {
    return false;
  }

  // Don't capture if already inside the target app scope.
  if (app_type == apps::mojom::AppType::kWeb) {
    auto* tab_helper = web_app::WebAppTabHelper::FromWebContents(web_contents);
    if (tab_helper && tab_helper->GetAppId() == preferred_app_id.value())
      return false;
  }

  // If this is a prerender navigation that would otherwise launch an app, we
  // must cancel it. We only want to launch an app once the URL is
  // intentionally navigated to by the user. We cancel the navigation here so
  // that when the link is clicked, we'll run NavigationThrottles again. If we
  // leave the prerendering alive, the activating navigation won't run
  // throttles.
  if (handle->IsInPrerenderedMainFrame())
    return true;

  auto launch_source = navigate_from_link()
                           ? apps::mojom::LaunchSource::kFromLink
                           : apps::mojom::LaunchSource::kFromOmnibox;
  GURL redirected_url =
      RedirectUrlIfSwa(profile, preferred_app_id.value(), url);
  proxy->LaunchAppWithUrl(
      preferred_app_id.value(),
      GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                    WindowOpenDisposition::NEW_WINDOW,
                    /*prefer_container=*/true),
      redirected_url, launch_source,
      apps::MakeWindowInfo(display::kDefaultDisplayId));

  const GURL& last_committed_url = web_contents->GetLastCommittedURL();
  if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank())
    web_contents->ClosePage();

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      GetMetricsPlatform(app_type));
  return true;
}

bool CommonAppsNavigationThrottle::ShouldShowDisablePage(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::vector<std::string> app_ids =
      apps::AppServiceProxyFactory::GetForProfile(profile)->GetAppIdsForUrl(
          url, /*exclude_browser=*/true, /*exclude_browser_tab_apps=*/false);

  for (auto app_id : app_ids) {
    if (IsAppDisabled(app_id)) {
      return true;
    }
  }
  return false;
}

ThrottleCheckResult CommonAppsNavigationThrottle::MaybeShowCustomResult() {
  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_ADMINISTRATOR,
                             GetAppDisabledErrorPage());
}

}  // namespace apps
