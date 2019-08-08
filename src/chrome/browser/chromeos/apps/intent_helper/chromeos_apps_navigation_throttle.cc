// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace chromeos {

// static
std::unique_ptr<apps::AppsNavigationThrottle>
ChromeOsAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();
  const bool arc_enabled = arc::IsArcPlayStoreEnabledForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!arc_enabled && !apps::AppsNavigationThrottle::CanCreate(web_contents))
    return nullptr;

  return std::make_unique<ChromeOsAppsNavigationThrottle>(handle, arc_enabled);
}

// static
void ChromeOsAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  arc::ArcIntentPickerAppFetcher::GetArcAppsForPicker(
      web_contents, url,
      base::BindOnce(&ChromeOsAppsNavigationThrottle::
                         FindPwaForUrlAndShowIntentPickerForApps,
                     web_contents, ui_auto_display_service, url));
}

// static
void ChromeOsAppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    apps::mojom::AppType app_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      if (arc::ArcIntentPickerAppFetcher::MaybeLaunchOrPersistArcApp(
              url, launch_name, should_launch_app, should_persist)) {
        CloseOrGoBack(web_contents);
      } else {
        close_reason = apps::IntentPickerCloseReason::PICKER_ERROR;
      }
      return;
    case apps::mojom::AppType::kUnknown:
      // TODO(crbug.com/826982): This workaround can be removed when preferences
      // are no longer persisted within the ARC container, it was necessary
      // since chrome browser is neither a PWA or ARC app.
      if (close_reason == apps::IntentPickerCloseReason::STAY_IN_CHROME &&
          should_persist) {
        arc::ArcIntentPickerAppFetcher::MaybeLaunchOrPersistArcApp(
            url, launch_name, /*should_launch_app=*/false,
            /*should_persist=*/true);
      }
      // Fall through to super class method to increment counter.
      break;
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kExtension:
      break;
  }
  apps::AppsNavigationThrottle::OnIntentPickerClosed(
      web_contents, ui_auto_display_service, url, launch_name, app_type,
      close_reason, should_persist);
}

// static
void ChromeOsAppsNavigationThrottle::RecordUma(
    const std::string& selected_app_package,
    apps::mojom::AppType app_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (app_type == apps::mojom::AppType::kArc &&
      (close_reason == apps::IntentPickerCloseReason::PREFERRED_APP_FOUND ||
       close_reason == apps::IntentPickerCloseReason::OPEN_APP)) {
    UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                              arc::UserInteractionType::APP_STARTED_FROM_LINK);
  }
  apps::AppsNavigationThrottle::RecordUma(selected_app_package, app_type,
                                          close_reason, should_persist);
}

ChromeOsAppsNavigationThrottle::ChromeOsAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    bool arc_enabled)
    : apps::AppsNavigationThrottle(navigation_handle),
      arc_enabled_(arc_enabled),
      weak_factory_(this) {}

ChromeOsAppsNavigationThrottle::~ChromeOsAppsNavigationThrottle() = default;

// static
void ChromeOsAppsNavigationThrottle::FindPwaForUrlAndShowIntentPickerForApps(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<apps::IntentPickerAppInfo> apps) {
  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps_for_picker),
      /*show_remember_selection=*/true,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

// static
apps::AppsNavigationThrottle::Platform
ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_launch_name,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::PREFERRED_ACTIVITY_FOUND:
      return arc::ArcIntentHelperBridge::IsIntentHelperPackage(
                 selected_launch_name)
                 ? Platform::CHROME
                 : Platform::ARC;
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
    case PickerAction::PWA_APP_PRESSED:
    case PickerAction::PICKER_ERROR:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
      break;
  }
  return apps::AppsNavigationThrottle::GetDestinationPlatform(
      selected_launch_name, picker_action);
}

// Removes the flag signaling that the current tab was started via
// ChromeShellDelegate if the current throttle corresponds to a navigation
// passing thru different domains or schemes, except if |current_url| has a
// scheme different than http(s).
void ChromeOsAppsNavigationThrottle::MaybeRemoveComingFromArcFlag(
    content::WebContents* web_contents,
    const GURL& previous_url,
    const GURL& current_url) {
  // Let ArcExternalProtocolDialog handle these cases.
  if (!current_url.SchemeIsHTTPOrHTTPS())
    return;

  if (url::Origin::Create(previous_url)
          .IsSameOriginWith(url::Origin::Create(current_url))) {
    return;
  }

  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;
  arc::ArcWebContentsData* arc_data =
      static_cast<arc::ArcWebContentsData*>(web_contents->GetUserData(key));
  if (arc_data)
    web_contents->RemoveUserData(key);
}

void ChromeOsAppsNavigationThrottle::CancelNavigation() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents && web_contents->GetController().IsInitialNavigation()) {
    // Workaround for b/79167225, closing |web_contents| here may be dangerous.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ChromeOsAppsNavigationThrottle::CloseTab,
                       weak_factory_.GetWeakPtr()));
  } else {
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  }
}

bool ChromeOsAppsNavigationThrottle::ShouldDeferNavigationForArc(
    content::NavigationHandle* handle) {
  // Query for ARC apps, and if we are handling a link navigation, allow the
  // preferred app (if it exists) to be launched.
  if (arc_enabled_ &&
      arc::ArcIntentPickerAppFetcher::WillGetArcAppsForNavigation(
          handle,
          base::BindOnce(
              &ChromeOsAppsNavigationThrottle::OnDeferredNavigationProcessed,
              weak_factory_.GetWeakPtr()),
          /*should_launch_preferred_app=*/navigate_from_link())) {
    return true;
  }
  return false;
}

void ChromeOsAppsNavigationThrottle::OnDeferredNavigationProcessed(
    apps::AppsNavigationAction action,
    std::vector<apps::IntentPickerAppInfo> apps) {
  if (action == apps::AppsNavigationAction::CANCEL) {
    // We found a preferred ARC app to open; cancel the navigation and don't do
    // anything else.
    CancelNavigation();
    return;
  }

  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();

  std::vector<apps::IntentPickerAppInfo> apps_for_picker =
      FindPwaForUrl(web_contents, url, std::move(apps));

  ShowIntentPickerForApps(
      web_contents, ui_auto_display_service_, url, std::move(apps_for_picker),
      GetOnPickerClosedCallback(web_contents, ui_auto_display_service_, url));

  // We are about to resume the navigation, which may destroy this object.
  Resume();
}

apps::AppsNavigationThrottle::PickerShowState
ChromeOsAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return ShouldAutoDisplayUi(apps_for_picker, web_contents, url) &&
                 navigate_from_link()
             ? PickerShowState::kPopOut
             : PickerShowState::kOmnibox;
}

IntentPickerResponse ChromeOsAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

bool ChromeOsAppsNavigationThrottle::ShouldShowRememberSelection() {
  return true;
}

void ChromeOsAppsNavigationThrottle::CloseTab() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents)
    web_contents->ClosePage();
}

bool ChromeOsAppsNavigationThrottle::ShouldAutoDisplayUi(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  if (apps_for_picker.empty())
    return false;

  // If we only have PWAs in the app list, do not show the intent picker.
  // Instead just show the omnibox icon. This is to reduce annoyance to users
  // until "Remember my choice" is available for desktop PWAs.
  // TODO(crbug.com/826982): show the intent picker when the app registry is
  // available to persist "Remember my choice" for PWAs.
  bool only_pwa_apps =
      std::all_of(apps_for_picker.begin(), apps_for_picker.end(),
                  [](const apps::IntentPickerAppInfo& app_info) {
                    return app_info.type == apps::mojom::AppType::kWeb;
                  });
  if (only_pwa_apps)
    return false;

  DCHECK(ui_auto_display_service_);
  return ui_auto_display_service_->ShouldAutoDisplayUi(url);
}
}  // namespace chromeos
