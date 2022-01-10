// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/ui_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace safe_browsing {

SafeBrowsingUIManager::SafeBrowsingUIManager(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory,
    const GURL& default_safe_page)
    : delegate_(std::move(delegate)),
      blocking_page_factory_(std::move(blocking_page_factory)),
      default_safe_page_(default_safe_page) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::Stop(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (shutdown) {
    shut_down_ = true;
  }
}

void SafeBrowsingUIManager::CreateAndSendHitReport(
    const UnsafeResource& resource) {
  WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);
  DCHECK(web_contents);
  HitReport hit_report;
  hit_report.malicious_url = resource.url;
  hit_report.is_subresource = resource.is_subresource;
  hit_report.threat_type = resource.threat_type;
  hit_report.threat_source = resource.threat_source;
  hit_report.population_id = resource.threat_metadata.population_id;

  NavigationEntry* entry = GetNavigationEntryForResource(resource);
  if (entry) {
    hit_report.page_url = entry->GetURL();
    hit_report.referrer_url = entry->GetReferrer().url;
  }

  // When the malicious url is on the main frame, and resource.original_url
  // is not the same as the resource.url, that means we have a redirect from
  // resource.original_url to resource.url.
  // Also, at this point, page_url points to the _previous_ page that we
  // were on. We replace page_url with resource.original_url and referrer
  // with page_url.
  if (!resource.is_subresource && !resource.original_url.is_empty() &&
      resource.original_url != resource.url) {
    hit_report.referrer_url = hit_report.page_url;
    hit_report.page_url = resource.original_url;
  }

  const auto& prefs = *delegate_->GetPrefs(web_contents->GetBrowserContext());

  hit_report.extended_reporting_level = GetExtendedReportingLevel(prefs);
  hit_report.is_enhanced_protection = IsEnhancedProtectionEnabled(prefs);
  hit_report.is_metrics_reporting_active =
      delegate_->IsMetricsAndCrashReportingEnabled();

  MaybeReportSafeBrowsingHit(hit_report, web_contents);

  for (Observer& observer : observer_list_)
    observer.OnSafeBrowsingHit(resource);
}

void SafeBrowsingUIManager::StartDisplayingBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);

  if (!web_contents) {
    // Tab is gone.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/);
    return;
  }

  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      delegate_->GetNoStatePrefetchContentsIfExists(web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    // Tab is being prerendered.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/);
    return;
  }

  // Whether we have a FrameTreeNode id or a RenderFrameHost id depends on
  // whether SB was triggered for a frame navigation or a document's subresource
  // load respectively. We consider both cases here.
  const content::GlobalRenderFrameHostId rfh_id(resource.render_process_id,
                                                resource.render_frame_id);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(rfh_id);
  const bool is_prerender =
      web_contents->IsPrerenderedFrame(resource.frame_tree_node_id) ||
      (rfh && rfh->GetLifecycleState() ==
                  content::RenderFrameHost::LifecycleState::kPrerendering);

  if (is_prerender) {
    // TODO(mcnee): If we were to indicate that this does not show an
    // interstitial, the loader throttle would cancel with ERR_ABORTED to
    // suppress an error page, instead of blocking using ERR_BLOCKED_BY_CLIENT.
    // Prerendering code needs to distiguish these cases, so we pretend that
    // we've shown an interstitial to get a meaningful error code.
    // Given that the only thing the |showed_interstitial| parameter is used for
    // is controlling the error code, perhaps this should be renamed to better
    // indicate its purpose.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              true /*showed_interstitial*/);
    return;
  }

  // We don't show interstitials for extension triggered SB errors, since they
  // might not be visible, and cause the extension to hang. The request is just
  // cancelled instead.
  if (delegate_->IsHostingExtension(web_contents)) {
    resource.DispatchCallback(FROM_HERE, false /* proceed */,
                              false /* showed_interstitial */);
    return;
  }

  // With committed interstitials, if this is a main frame load, we need to
  // get the navigation URL and referrer URL from the navigation entry now,
  // since they are required for threat reporting, and the entry will be
  // destroyed once the request is failed.
  if (resource.IsMainPageLoadBlocked()) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    if (entry) {
      security_interstitials::UnsafeResource resource_copy(resource);
      resource_copy.navigation_url = entry->GetURL();
      resource_copy.referrer_url = entry->GetReferrer().url;
      DisplayBlockingPage(resource_copy);
      return;
    }
  }
  DisplayBlockingPage(resource);
}

bool SafeBrowsingUIManager::ShouldSendHitReport(const HitReport& hit_report,
                                                WebContents* web_contents) {
  return web_contents &&
         hit_report.extended_reporting_level != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord() &&
         delegate_->IsSendingOfHitReportsEnabled();
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// extended-reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Send report if user opted-in to extended reporting and is not in
  //  incognito mode.
  if (!ShouldSendHitReport(hit_report, web_contents))
    return;

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (shut_down_ || !delegate_->GetPingManagerIfExists())
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << hit_report.malicious_url << " "
           << hit_report.page_url << " " << hit_report.referrer_url << " "
           << hit_report.is_subresource << " " << hit_report.threat_type;
  delegate_->GetPingManagerIfExists()->ReportSafeBrowsingHit(
      delegate_->GetURLLoaderFactory(web_contents->GetBrowserContext()),
      hit_report);
}

// Static.
void SafeBrowsingUIManager::CreateAllowlistForTesting(
    content::WebContents* web_contents) {
  EnsureAllowlistCreated(web_contents);
}

// static
std::string SafeBrowsingUIManager::GetThreatTypeStringForInterstitial(
    safe_browsing::SBThreatType threat_type) {
  switch (threat_type) {
    case safe_browsing::SB_THREAT_TYPE_URL_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return "SOCIAL_ENGINEERING";
    case safe_browsing::SB_THREAT_TYPE_URL_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return "MALWARE";
    case safe_browsing::SB_THREAT_TYPE_URL_UNWANTED:
      return "UNWANTED_SOFTWARE";
    case safe_browsing::SB_THREAT_TYPE_BILLING:
      return "THREAT_TYPE_UNSPECIFIED";
    case safe_browsing::SB_THREAT_TYPE_UNUSED:
    case safe_browsing::SB_THREAT_TYPE_SAFE:
    case safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_EXTENSION:
    case safe_browsing::SB_THREAT_TYPE_BLOCKLISTED_RESOURCE:
    case safe_browsing::SB_THREAT_TYPE_API_ABUSE:
    case safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case safe_browsing::SB_THREAT_TYPE_CSD_ALLOWLIST:
    case safe_browsing::
        DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_APK_DOWNLOAD:
    case safe_browsing::SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case safe_browsing::SB_THREAT_TYPE_ACCURACY_TIPS:
      NOTREACHED();
      break;
  }
  return std::string();
}
void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

const std::string SafeBrowsingUIManager::app_locale() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return delegate_->GetApplicationLocale();
}

history::HistoryService* SafeBrowsingUIManager::history_service(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return delegate_->GetHistoryService(web_contents->GetBrowserContext());
}

const GURL SafeBrowsingUIManager::default_safe_page() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return default_safe_page_;
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedThreatDetails(
    content::BrowserContext* browser_context,
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (shut_down_ || !delegate_->GetPingManagerIfExists())
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details.";
    delegate_->GetPingManagerIfExists()->ReportThreatDetails(
        delegate_->GetURLLoaderFactory(browser_context), serialized);
  }
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    bool showed_interstitial) {
  BaseUIManager::OnBlockingPageDone(resources, proceed, web_contents,
                                    main_frame_url, showed_interstitial);
  if (proceed && !resources.empty()) {
    delegate_->TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        web_contents, main_frame_url,
        GetThreatTypeStringForInterstitial(resources[0].threat_type),
        /*net_error_code=*/0);
  }
}
// Static.
GURL SafeBrowsingUIManager::GetMainFrameAllowlistUrlForResourceForTesting(
    const security_interstitials::UnsafeResource& resource) {
  return GetMainFrameAllowlistUrlForResource(resource);
}

BaseBlockingPage* SafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  SafeBrowsingBlockingPage* blocking_page =
      blocking_page_factory_->CreateSafeBrowsingPage(
          this, contents, blocked_url, {unsafe_resource},
          /*should_trigger_reporting=*/true);

  // Report that we showed an interstitial.
  ForwardSecurityInterstitialShownExtensionEventToEmbedder(
      contents, blocked_url,
      GetThreatTypeStringForInterstitial(unsafe_resource.threat_type),
      /*net_error_code=*/0);

  return blocking_page;
}

void SafeBrowsingUIManager::
    ForwardSecurityInterstitialShownExtensionEventToEmbedder(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  delegate_->TriggerSecurityInterstitialShownExtensionEventIfDesired(
      web_contents, page_url, reason, net_error_code);
}

}  // namespace safe_browsing
