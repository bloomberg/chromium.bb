// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_browser_agent.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/app_launcher/app_launcher_util.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(AppLauncherBrowserAgent)

using app_launcher_overlays::AppLaunchConfirmationRequest;
using app_launcher_overlays::AllowAppLaunchResponse;

namespace {
// Records histogram metric on the user's response when prompted to open another
// application. |user_accepted| should be YES if the user accepted the prompt to
// launch another application. This call is extracted to a separate function to
// reduce macro code expansion.
void RecordUserAcceptedAppLaunchMetric(BOOL user_accepted) {
  UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened", user_accepted);
}

// Callback for the app launcher alert overlay.
void AppLauncherOverlayCallback(base::OnceCallback<void(bool)> completion,
                                bool repeated_request,
                                OverlayResponse* response) {
  // Check whether the user has allowed the navigation.
  bool user_accepted = response && response->GetInfo<AllowAppLaunchResponse>();

  // Record the UMA for repeated requests.
  if (repeated_request)
    RecordUserAcceptedAppLaunchMetric(user_accepted);

  // Execute the completion with the response.
  DCHECK(!completion.is_null());
  std::move(completion).Run(user_accepted);
}

// Launches the app for |url| if |user_accepted| is true.
void LaunchExternalApp(const GURL url, bool user_accepted = true) {
  if (!user_accepted)
    return;
  [[UIApplication sharedApplication] openURL:net::NSURLWithGURL(url)
                                     options:@{}
                           completionHandler:nil];
}
}  // namespace

#pragma mark - AppLauncherBrowserAgent

AppLauncherBrowserAgent::AppLauncherBrowserAgent(Browser* browser)
    : tab_helper_delegate_(browser->GetWebStateList()),
      tab_helper_delegate_installer_(&tab_helper_delegate_, browser) {}

AppLauncherBrowserAgent::~AppLauncherBrowserAgent() = default;

#pragma mark - AppLauncherBrowserAgent::TabHelperDelegate

AppLauncherBrowserAgent::TabHelperDelegate::TabHelperDelegate(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
}

AppLauncherBrowserAgent::TabHelperDelegate::~TabHelperDelegate() = default;

#pragma mark AppLauncherTabHelperDelegate

void AppLauncherBrowserAgent::TabHelperDelegate::LaunchAppForTabHelper(
    AppLauncherTabHelper* tab_helper,
    const GURL& url,
    bool link_transition) {
  // Don't open application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return;
  }

  // Uses a Mailto Handler to open the appropriate app.
  if (url.SchemeIs(url::kMailToScheme)) {
    MailtoHandlerProvider* provider =
        ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
    provider->HandleMailtoURL(net::NSURLWithGURL(url));
    return;
  }

  // Show the a dialog for app store launches and external URL navigations that
  // did not originate from a link tap.
  bool show_dialog = UrlHasAppStoreScheme(url) || !link_transition;
  if (show_dialog) {
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
            /*is_repeated_request=*/false);
    request->GetCallbackManager()->AddCompletionCallback(base::BindOnce(
        &AppLauncherOverlayCallback, base::BindOnce(&LaunchExternalApp, url),
        /*repeated_request=*/false));
    GetQueueForAppLaunchDialog(tab_helper->web_state())
        ->AddRequest(std::move(request));
  } else {
    LaunchExternalApp(url);
  }
}

void AppLauncherBrowserAgent::TabHelperDelegate::ShowRepeatedAppLaunchAlert(
    AppLauncherTabHelper* tab_helper,
    base::OnceCallback<void(bool)> completion) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          /*is_repeated_request=*/true);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&AppLauncherOverlayCallback, std::move(completion),
                     /*is_repeated_request=*/true));
  GetQueueForAppLaunchDialog(tab_helper->web_state())
      ->AddRequest(std::move(request));
}

#pragma mark Private

OverlayRequestQueue*
AppLauncherBrowserAgent::TabHelperDelegate::GetQueueForAppLaunchDialog(
    web::WebState* web_state) {
  web::WebState* queue_web_state = web_state;
  // If an app launch navigation is occurring in a new tab, the tab will be
  // closed immediately after the navigation fails, cancelling the app launcher
  // dialog before it gets a chance to be shown.  When this occurs, use the
  // OverlayRequestQueue for the tab's opener instead.
  if (!web_state->GetNavigationManager()->GetItemCount() &&
      web_state->HasOpener()) {
    int index = web_state_list_->GetIndexOfWebState(web_state);
    queue_web_state =
        web_state_list_->GetOpenerOfWebStateAt(index).opener ?: queue_web_state;
  }
  return OverlayRequestQueue::FromWebState(queue_web_state,
                                           OverlayModality::kWebContentArea);
}
