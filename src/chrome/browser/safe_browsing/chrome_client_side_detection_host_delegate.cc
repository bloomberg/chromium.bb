// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"

namespace safe_browsing {

namespace {
// The number of user gestures we trace back for CSD attribution.
const int kCSDAttributionUserGestureLimitForExtendedReporting = 5;
}  // namespace

// static
std::unique_ptr<ClientSideDetectionHost>
ChromeClientSideDetectionHostDelegate::CreateHost(content::WebContents* tab) {
  content::BrowserContext* browser_context = tab->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return ClientSideDetectionHost::Create(
      tab, std::make_unique<ChromeClientSideDetectionHostDelegate>(tab),
      profile->GetPrefs(),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      profile->IsOffTheRecord(),
      base::BindRepeating(&safe_browsing::SyncUtils::IsPrimaryAccountSignedIn,
                          IdentityManagerFactory::GetForProfile(profile)));
}

ChromeClientSideDetectionHostDelegate::ChromeClientSideDetectionHostDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}
ChromeClientSideDetectionHostDelegate::
    ~ChromeClientSideDetectionHostDelegate() = default;

bool ChromeClientSideDetectionHostDelegate::
    HasSafeBrowsingUserInteractionObserver() {
  return SafeBrowsingUserInteractionObserver::FromWebContents(web_contents_);
}

PrefService* ChromeClientSideDetectionHostDelegate::GetPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? profile->GetPrefs() : nullptr;
}

scoped_refptr<SafeBrowsingDatabaseManager>
ChromeClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->database_manager().get() : nullptr;
}

SafeBrowsingNavigationObserverManager* ChromeClientSideDetectionHostDelegate::
    GetSafeBrowsingNavigationObserverManager() {
  if (observer_manager_for_testing_) {
    return observer_manager_for_testing_;
  }

  return SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

scoped_refptr<BaseUIManager>
ChromeClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->ui_manager() : nullptr;
}

ClientSideDetectionService*
ChromeClientSideDetectionHostDelegate::GetClientSideDetectionService() {
  return ClientSideDetectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ChromeClientSideDetectionHostDelegate::AddReferrerChain(
    ClientPhishingRequest* verdict,
    GURL current_url) {
  SafeBrowsingNavigationObserverManager* navigation_observer_manager =
      GetSafeBrowsingNavigationObserverManager();
  if (!navigation_observer_manager) {
    return;
  }
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager->IdentifyReferrerChainByEventURL(
          current_url, SessionID::InvalidValue(),
          kCSDAttributionUserGestureLimitForExtendedReporting,
          verdict->mutable_referrer_chain());

  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.ReferrerURLChainSize.CSDAttribution",
                           verdict->referrer_chain().size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.CSDAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  // Determines how many recent navigation events to append to referrer
  // chain if any.
  size_t recent_navigations_to_collect =
      CountOfRecentNavigationsToAppend(result);

  navigation_observer_manager->AppendRecentNavigations(
      recent_navigations_to_collect, verdict->mutable_referrer_chain());
}

size_t ChromeClientSideDetectionHostDelegate::CountOfRecentNavigationsToAppend(
    SafeBrowsingNavigationObserverManager::AttributionResult result) {
  auto* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return web_contents_ ? SafeBrowsingNavigationObserverManager::
                             CountOfRecentNavigationsToAppend(
                                 profile, profile->GetPrefs(), result)
                       : 0u;
}

}  // namespace safe_browsing
