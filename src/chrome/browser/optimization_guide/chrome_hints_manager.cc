// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_hints_manager.h"

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/optimization_guide/android/android_push_notification_manager.h"
#endif

namespace {

// Returns true if we can make a request for hints for |prediction|.
bool IsAllowedToFetchForNavigationPrediction(
    const absl::optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK(prediction);

  if (prediction->prediction_source() !=
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    // We only support predictions from page anchors.
    return false;
  }
  const absl::optional<GURL> source_document_url =
      prediction->source_document_url();
  if (!source_document_url || source_document_url->is_empty())
    return false;

  // We only extract next predicted navigations from Google SRP URLs.
  return google_util::IsGoogleSearchUrl(*source_document_url);
}

}  // namespace

namespace optimization_guide {

ChromeHintsManager::ChromeHintsManager(
    Profile* profile,
    PrefService* pref_service,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    optimization_guide::TopHostProvider* top_host_provider,
    optimization_guide::TabUrlProvider* tab_url_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<optimization_guide::PushNotificationManager>
        push_notification_manager)
    : HintsManager(profile->IsOffTheRecord(),
                   g_browser_process->GetApplicationLocale(),
                   pref_service,
                   hint_store,
                   top_host_provider,
                   tab_url_provider,
                   url_loader_factory,
                   network_connection_tracker,
                   std::move(push_notification_manager)),
      profile_(profile) {
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile);
  if (navigation_predictor_service)
    navigation_predictor_service->AddObserver(this);
}

ChromeHintsManager::~ChromeHintsManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ChromeHintsManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HintsManager::Shutdown();

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service)
    navigation_predictor_service->RemoveObserver(this);
}

void ChromeHintsManager::OnPredictionUpdated(
    const absl::optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prediction);

  if (!IsAllowedToFetchForNavigationPrediction(prediction))
    return;

  // Per comments in NavigationPredictorKeyedService::Prediction, this pointer
  // should be valid while OnPredictionUpdated is on the call stack.
  content::WebContents* web_contents = prediction->web_contents();
  DCHECK(web_contents);
  auto* observer =
      OptimizationGuideWebContentsObserver::FromWebContents(web_contents);
  DCHECK(observer);

  std::vector<GURL> urls_to_fetch;
  for (const auto& url : prediction->sorted_predicted_urls()) {
    if (!IsAllowedToFetchNavigationHints(url))
      continue;
    // Don't prefetch hints for SRP links that point back to Google.
    if (google_util::IsGoogleSearchUrl(url))
      continue;

    if (hint_cache()->HasURLKeyedEntryForURL(url))
      continue;

    urls_to_fetch.push_back(url);
  }
  observer->AddURLsToBatchFetchBasedOnPrediction(std::move(urls_to_fetch),
                                                 web_contents);
}
}  // namespace optimization_guide
