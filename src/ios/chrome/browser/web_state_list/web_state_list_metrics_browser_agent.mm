// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_metrics_browser_agent.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#include "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebStateListMetricsBrowserAgent)

// static
void WebStateListMetricsBrowserAgent::CreateForBrowser(
    Browser* browser,
    SessionMetrics* session_metrics) {
  if (!FromBrowser(browser)) {
    browser->SetUserData(UserDataKey(),
                         base::WrapUnique(new WebStateListMetricsBrowserAgent(
                             browser, session_metrics)));
  }
}

WebStateListMetricsBrowserAgent::WebStateListMetricsBrowserAgent(
    Browser* browser,
    SessionMetrics* session_metrics)
    : web_state_list_(browser->GetWebStateList()),
      session_metrics_(session_metrics) {
  DCHECK(web_state_list_);
  DCHECK(session_metrics_);
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
  web_state_forwarder_.reset(
      new AllWebStateObservationForwarder(web_state_list_, this));

  SessionRestorationBrowserAgent* restoration_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);
  if (restoration_agent)
    restoration_agent->AddObserver(this);
}

WebStateListMetricsBrowserAgent::~WebStateListMetricsBrowserAgent() = default;

void WebStateListMetricsBrowserAgent::WillStartSessionRestoration() {
  metric_collection_paused_ = true;
}

void WebStateListMetricsBrowserAgent::SessionRestorationFinished(
    const std::vector<web::WebState*>& restored_web_states) {
  metric_collection_paused_ = false;
}

void WebStateListMetricsBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
  session_metrics_->OnWebStateInserted();
}

void WebStateListMetricsBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  session_metrics_->OnWebStateDetached();
}

void WebStateListMetricsBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (metric_collection_paused_)
    return;
  session_metrics_->OnWebStateActivated();
  if (reason == ActiveWebStateChangeReason::Replaced)
    return;

  base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
}

// web::WebStateObserver
void WebStateListMetricsBrowserAgent::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // In order to avoid false positive in the crash loop detection, disable the
  // counter as soon as an URL is loaded. This requires an user action and is a
  // significant source of crashes. Ignore NTP as it is loaded by default after
  // a crash.
  if (navigation_context->GetUrl().host_piece() != kChromeUINewTabHost) {
    static dispatch_once_t dispatch_once_token;
    dispatch_once(&dispatch_once_token, ^{
      crash_util::ResetFailedStartupAttemptCount();
    });
  }
}

void WebStateListMetricsBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted())
    return;

  if (!navigation_context->IsSameDocument() &&
      !web_state->GetBrowserState()->IsOffTheRecord()) {
    int tab_count = static_cast<int>(web_state_list_->count());
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);
  }

  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  navigation_metrics::RecordPrimaryMainFrameNavigation(
      item ? item->GetVirtualURL() : GURL::EmptyGURL(),
      navigation_context->IsSameDocument(),
      web_state->GetBrowserState()->IsOffTheRecord(),
      profile_metrics::GetBrowserProfileType(web_state->GetBrowserState()));
}

void WebStateListMetricsBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  switch (GetInterfaceOrientation()) {
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationPortraitUpsideDown:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", YES);
      break;
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", NO);
      break;
    case UIInterfaceOrientationUnknown:
      // TODO(crbug.com/228832): Convert from a boolean histogram to an
      // enumerated histogram and log this case as well.
      break;
  }
}

void WebStateListMetricsBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);

  SessionRestorationBrowserAgent* restoration_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);
  if (restoration_agent)
    restoration_agent->RemoveObserver(this);

  web_state_forwarder_.reset(nullptr);
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;

  browser->RemoveObserver(this);
}
