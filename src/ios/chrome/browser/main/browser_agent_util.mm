// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_agent_util.h"

#include "base/feature_list.h"
#include "components/breadcrumbs/core/features.h"
#import "ios/chrome/browser/app_launcher/app_launcher_browser_agent.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/device_sharing/device_sharing_browser_agent.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/sessions/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/tabs/synced_window_delegate_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_parenting_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#include "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/view_source_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list_metrics_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachBrowserAgents(Browser* browser) {
  if (base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::CreateForBrowser(browser);
  }
  LiveTabContextBrowserAgent::CreateForBrowser(browser);
  TabInsertionBrowserAgent::CreateForBrowser(browser);
  AttachInfobarOverlayBrowserAgent(browser);
  SyncedWindowDelegateBrowserAgent::CreateForBrowser(browser);
  WebUsageEnablerBrowserAgent::CreateForBrowser(browser);
  DeviceSharingBrowserAgent::CreateForBrowser(browser);
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser);
  AppLauncherBrowserAgent::CreateForBrowser(browser);
  WebNavigationBrowserAgent::CreateForBrowser(browser);
  TabParentingBrowserAgent::CreateForBrowser(browser);

  ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser);
  SnapshotBrowserAgent::CreateForBrowser(browser);

  // PolicyWatcher is non-OTR only.
  if (!browser->GetBrowserState()->IsOffTheRecord())
    PolicyWatcherBrowserAgent::CreateForBrowser(browser);

  // Send Tab To Self is non-OTR only.
  if (!browser->GetBrowserState()->IsOffTheRecord())
    SendTabToSelfBrowserAgent::CreateForBrowser(browser);

  WebStateDelegateBrowserAgent::CreateForBrowser(
      browser, TabInsertionBrowserAgent::FromBrowser(browser));

  // ViewSourceBrowserAgent requires TabInsertionBrowserAgent, and is only used
  // in debug builds.
#if !defined(NDEBUG)
  ViewSourceBrowserAgent::CreateForBrowser(browser);
#endif  // !defined(NDEBUG)

  // UrlLoadingBrowserAgent requires UrlLoadingNotifierBrowserAgent.
  UrlLoadingBrowserAgent::CreateForBrowser(browser);

  // SessionRestorartionAgent requires WebUsageEnablerBrowserAgent.
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser, [SessionServiceIOS sharedService]);

  // TabUsageRecorderBrowserAgent and WebStateListMetricsBrowserAgent observe
  // the SessionRestorationBrowserAgent, so they should be created after the the
  // SessionRestorationBrowserAgent is created.
  WebStateListMetricsBrowserAgent::CreateForBrowser(
      browser, SessionMetrics::FromBrowserState(browser->GetBrowserState()));

  // Normal browser states are the only ones to get tab usage recorder.
  if (!browser->GetBrowserState()->IsOffTheRecord())
    TabUsageRecorderBrowserAgent::CreateForBrowser(browser);

  if (!browser->GetBrowserState()->IsOffTheRecord())
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser);

  // This needs to be called last in case any downstream browser agents need to
  // access upstream agents created earlier in this function.
  ios::GetChromeBrowserProvider().AttachBrowserAgents(browser);
}
