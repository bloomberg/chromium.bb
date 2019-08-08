// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace {

// Compares the host name of the referrer and target URL to decide whether
// the navigation needs to be overridden.
bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url) {
  // When the navigation is initiated in a web page where sending a referrer
  // is disabled, |previous_url| can be empty. In this case, we should open
  // it in the desktop browser.
  if (!previous_url.is_valid() || previous_url.is_empty())
    return false;

  // Also check |current_url| just in case.
  if (!current_url.is_valid() || current_url.is_empty()) {
    DVLOG(1) << "Unexpected URL: " << current_url << ", opening it in Chrome.";
    return false;
  }

  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (!current_url.SchemeIsHTTPOrHTTPS() ||
      previous_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  return true;
}

GURL GetStartingGURL(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preferrence: Referrer > LastCommittedURL > SiteURL,
  // GetSiteURL *should* only be used on very rare cases, e.g. when the
  // navigation goes from https: to http: on a new tab, thus losing the other
  // potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  return navigation_handle->GetStartingSiteInstance()->GetSiteURL();
}

bool IsDesktopPwasEnabled() {
  return base::FeatureList::IsEnabled(features::kDesktopPWAWindowing);
}

}  // namespace

namespace apps {

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  content::WebContents* web_contents = handle->GetWebContents();
  if (!CanCreate(web_contents))
    return nullptr;

  return std::make_unique<AppsNavigationThrottle>(handle);
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  std::vector<IntentPickerAppInfo> apps = FindPwaForUrl(web_contents, url, {});

  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_remember_selection=*/false,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

// static
void AppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    apps::mojom::AppType app_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == IntentPickerCloseReason::OPEN_APP;
  switch (app_type) {
    case apps::mojom::AppType::kWeb:
      if (should_launch_app) {
        const extensions::Extension* extension =
            extensions::ExtensionRegistry::Get(
                web_contents->GetBrowserContext())
                ->GetExtensionById(launch_name,
                                   extensions::ExtensionRegistry::ENABLED);
        DCHECK(extension);
        ReparentWebContentsIntoAppBrowser(web_contents, extension);
      }
      break;
    case apps::mojom::AppType::kUnknown:
      // We reach here if the picker was closed without an app being chosen,
      // e.g. due to the tab being closed. Keep count of this scenario so we can
      // stop the UI from showing after 2+ dismissals.
      if (close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED) {
        if (ui_auto_display_service)
          ui_auto_display_service->IncrementCounter(url);
      }
      break;
    case apps::mojom::AppType::kArc:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kExtension:
      NOTREACHED();
  }
  RecordUma(launch_name, app_type, close_reason, should_persist);
}

// static
void AppsNavigationThrottle::RecordUma(const std::string& selected_app_package,
                                       apps::mojom::AppType app_type,
                                       IntentPickerCloseReason close_reason,
                                       bool should_persist) {
  PickerAction action = GetPickerAction(app_type, close_reason, should_persist);
  Platform platform = GetDestinationPlatform(selected_app_package, action);

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerAction", action);

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.Apps.IntentPickerDestinationPlatform",
                            platform);
}

// static
bool AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
    const GURL& previous_url,
    const GURL& current_url) {
  return ShouldOverrideUrlLoading(previous_url, current_url);
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
    content::WebContents* web_contents,
    std::vector<IntentPickerAppInfo> apps,
    bool show_remember_selection,
    IntentPickerResponse callback) {
  if (apps.empty())
    return;

  // It should be safe to bind |web_contents| since closing the current tab will
  // close the intent picker and run the callback prior to the WebContents being
  // deallocated.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  browser->window()->ShowIntentPickerBubble(std::move(apps),
                                            /*show_stay_in_chrome=*/true,
                                            show_remember_selection,
                                            std::move(callback));
}

AppsNavigationThrottle::AppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      ui_displayed_(false),
      ui_auto_display_service_(
          IntentPickerAutoDisplayService::Get(Profile::FromBrowserContext(
              navigation_handle->GetWebContents()->GetBrowserContext()))),
      navigate_from_link_(false) {
  // |ui_auto_display_service_| can be null iff the call is coming from
  // IntentPickerView. Since the pointer to our service is never modified
  // (in case it is successfully created here) this check covers all the
  // non-static methods in this class.
  DCHECK(ui_auto_display_service_);
}

AppsNavigationThrottle::~AppsNavigationThrottle() = default;

const char* AppsNavigationThrottle::GetNameForLogging() {
  return "AppsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingGURL(navigation_handle());
  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle()->GetWebContents());
  if (browser)
    browser->window()->SetIntentPickerViewVisibility(/*visible=*/false);
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(dominickn): Consider what to do when there is another URL during the
  // same navigation that could be handled by apps. Two ideas are:
  //  1) update the bubble with a mix of both app candidates (if different)
  //  2) show a bubble based on the last url, thus closing all the previous ones
  if (ui_displayed_)
    return content::NavigationThrottle::PROCEED;

  return HandleRequest();
}

// static
bool AppsNavigationThrottle::CanCreate(content::WebContents* web_contents) {
  // Do not create the throttle if no apps can be installed.
  if (!IsDesktopPwasEnabled())
    return false;

  // Do not create the throttle in incognito or for a prerender navigation.
  if (web_contents->GetBrowserContext()->IsOffTheRecord() ||
      prerender::PrerenderContents::FromWebContents(web_contents) != nullptr) {
    return false;
  }

  // Do not create the throttle if there is no browser for the WebContents or we
  // are already in an app browser. The former can happen if an initial
  // navigation is reparented into a new app browser instance.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || browser->is_app())
    return false;

  return true;
}

// static
AppsNavigationThrottle::Platform AppsNavigationThrottle::GetDestinationPlatform(
    const std::string& selected_launch_name,
    PickerAction picker_action) {
  switch (picker_action) {
    case PickerAction::ARC_APP_PRESSED:
    case PickerAction::ARC_APP_PREFERRED_PRESSED:
      return Platform::ARC;
    case PickerAction::PWA_APP_PRESSED:
      return Platform::PWA;
    case PickerAction::PICKER_ERROR:
    case PickerAction::DIALOG_DEACTIVATED:
    case PickerAction::CHROME_PRESSED:
    case PickerAction::CHROME_PREFERRED_PRESSED:
      return Platform::CHROME;
    case PickerAction::PREFERRED_ACTIVITY_FOUND:
    case PickerAction::OBSOLETE_ALWAYS_PRESSED:
    case PickerAction::OBSOLETE_JUST_ONCE_PRESSED:
    case PickerAction::INVALID:
      break;
  }
  NOTREACHED();
  return Platform::ARC;
}

// static
std::vector<IntentPickerAppInfo> AppsNavigationThrottle::FindPwaForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
  if (IsDesktopPwasEnabled()) {
    // Check if the current URL has an installed desktop PWA, and add that to
    // the list of apps if it exists.
    const extensions::Extension* extension =
        extensions::util::GetInstalledPwaForUrl(
            web_contents->GetBrowserContext(), url,
            extensions::LAUNCH_CONTAINER_WINDOW);

    if (extension) {
      auto* menu_manager =
          extensions::MenuManager::Get(web_contents->GetBrowserContext());

      // Prefer the web and place apps of type PWA before apps of type ARC.
      // TODO(crbug.com/824598): deterministically sort this list.
      apps.emplace(apps.begin(), apps::mojom::AppType::kWeb,
                   menu_manager->GetIconForExtension(extension->id()),
                   extension->id(), extension->name());
    }
  }
  return apps;
}

// static
void AppsNavigationThrottle::CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack())
    web_contents->GetController().GoBack();
  else
    web_contents->ClosePage();
}

bool AppsNavigationThrottle::ShouldDeferNavigationForArc(
    content::NavigationHandle* handle) {
  return false;
}

void AppsNavigationThrottle::ShowIntentPickerForApps(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps,
    IntentPickerResponse callback) {
  if (apps.empty())
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  const PickerShowState picker_show_state =
      GetPickerShowState(apps, web_contents, url);
  switch (picker_show_state) {
    case PickerShowState::kOmnibox:
      ui_displayed_ = false;
      browser->window()->SetIntentPickerViewVisibility(true);
      break;
    case PickerShowState::kPopOut:
      ShowIntentPickerBubbleForApps(web_contents, std::move(apps),
                                    ShouldShowRememberSelection(),
                                    std::move(callback));
      break;
    default:
      NOTREACHED();
  }
}

AppsNavigationThrottle::PickerShowState
AppsNavigationThrottle::GetPickerShowState(
    const std::vector<IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse AppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

bool AppsNavigationThrottle::ShouldShowRememberSelection() {
  return false;
}

bool AppsNavigationThrottle::navigate_from_link() {
  return navigate_from_link_;
}

// static
AppsNavigationThrottle::PickerAction AppsNavigationThrottle::GetPickerAction(
    apps::mojom::AppType app_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  switch (close_reason) {
    case IntentPickerCloseReason::PICKER_ERROR:
      return PickerAction::PICKER_ERROR;
    case IntentPickerCloseReason::DIALOG_DEACTIVATED:
      return PickerAction::DIALOG_DEACTIVATED;
    case IntentPickerCloseReason::PREFERRED_APP_FOUND:
      return PickerAction::PREFERRED_ACTIVITY_FOUND;
    case IntentPickerCloseReason::STAY_IN_CHROME:
      return should_persist ? PickerAction::CHROME_PREFERRED_PRESSED
                            : PickerAction::CHROME_PRESSED;
    case IntentPickerCloseReason::OPEN_APP:
      switch (app_type) {
        case apps::mojom::AppType::kUnknown:
          return PickerAction::INVALID;
        case apps::mojom::AppType::kArc:
          return should_persist ? PickerAction::ARC_APP_PREFERRED_PRESSED
                                : PickerAction::ARC_APP_PRESSED;
        case apps::mojom::AppType::kWeb:
          return PickerAction::PWA_APP_PRESSED;
        case apps::mojom::AppType::kBuiltIn:
        case apps::mojom::AppType::kCrostini:
        case apps::mojom::AppType::kExtension:
          NOTREACHED();
      }
  }

  NOTREACHED();
  return PickerAction::INVALID;
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();
  // If the navigation happened without changing document or the
  // navigation resulted in an error page, don't check intent for the
  // navigation.
  if (handle->IsSameDocument() || handle->IsErrorPage())
    return content::NavigationThrottle::PROCEED;

  DCHECK(!ui_displayed_);

  navigate_from_link_ = false;

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = false;

  ui::PageTransition page_transition = handle->GetPageTransition();
  content::WebContents* web_contents = handle->GetWebContents();
  const GURL& url = handle->GetURL();
  if (!ShouldIgnoreNavigation(page_transition, kAllowFormSubmit,
                              kAllowClientRedirect) &&
      !handle->WasStartedFromContextMenu()) {
    navigate_from_link_ = true;
  }

  MaybeRemoveComingFromArcFlag(web_contents, starting_url_, url);

  if (!ShouldOverrideUrlLoading(starting_url_, url))
    return content::NavigationThrottle::PROCEED;

  if (ShouldDeferNavigationForArc(handle)) {
    // Handling is now deferred to ArcIntentPickerAppFetcher, which
    // asynchronously queries ARC for apps, and runs
    // OnDeferredNavigationProcessed() with an action based on whether an
    // acceptable app was found and user consent to open received. We assume the
    // UI is shown or a preferred app was found; reset to false if we resume the
    // navigation.
    ui_displayed_ = true;
    return content::NavigationThrottle::DEFER;
  }

  // We didn't query ARC, so proceed with the navigation and query if we have an
  // installed desktop PWA to handle the URL.
  if (IsDesktopPwasEnabled()) {
    std::vector<IntentPickerAppInfo> apps =
        FindPwaForUrl(web_contents, url, {});

    if (!apps.empty())
      ui_displayed_ = true;

    ShowIntentPickerForApps(
        web_contents, ui_auto_display_service_, url, std::move(apps),
        GetOnPickerClosedCallback(web_contents, ui_auto_display_service_, url));
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace apps
