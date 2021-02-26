// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/url_lookup_service.h"

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

RealTimeUrlLookupService::RealTimeUrlLookupService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    const ChromeUserPopulation::ProfileManagementStatus&
        profile_management_status,
    bool is_under_advanced_protection,
    bool is_off_the_record,
    variations::VariationsService* variations_service)
    : RealTimeUrlLookupServiceBase(url_loader_factory,
                                   cache_manager,
                                   sync_service,
                                   pref_service,
                                   profile_management_status,
                                   is_under_advanced_protection,
                                   is_off_the_record),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      pref_service_(pref_service),
      is_off_the_record_(is_off_the_record),
      variations_(variations_service) {
  token_fetcher_ =
      std::make_unique<SafeBrowsingTokenFetcher>(identity_manager_);
}

void RealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  token_fetcher_->Start(
      signin::ConsentLevel::kNotRequired,
      base::BindOnce(&RealTimeUrlLookupService::OnGetAccessToken,
                     weak_factory_.GetWeakPtr(), url,
                     std::move(request_callback), std::move(response_callback),
                     base::TimeTicks::Now()));
}

void RealTimeUrlLookupService::OnGetAccessToken(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    base::TimeTicks get_token_start_time,
    base::Optional<signin::AccessTokenInfo> access_token_info) {
  base::UmaHistogramTimes("SafeBrowsing.RT.GetToken.Time",
                          base::TimeTicks::Now() - get_token_start_time);
  base::UmaHistogramBoolean("SafeBrowsing.RT.HasTokenFromFetcher",
                            access_token_info.has_value());
  std::string access_token_string =
      access_token_info.value_or(signin::AccessTokenInfo()).token;
  SendRequest(url, access_token_string, std::move(request_callback),
              std::move(response_callback));
}

RealTimeUrlLookupService::~RealTimeUrlLookupService() {}

bool RealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformFullURLLookup(
      pref_service_, is_off_the_record_, variations_);
}

bool RealTimeUrlLookupService::CanPerformFullURLLookupWithToken() const {
  return RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
      pref_service_, is_off_the_record_, sync_service_, identity_manager_,
      variations_);
}

bool RealTimeUrlLookupService::CanCheckSubresourceURL() const {
  return IsEnhancedProtectionEnabled(*pref_service_);
}

bool RealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  // Always return true, because consumer real time URL check only works when
  // safe browsing is enabled.
  return true;
}

net::NetworkTrafficAnnotationTag
RealTimeUrlLookupService::GetTrafficAnnotationTag() const {
  return net::DefineNetworkTrafficAnnotation(
      "safe_browsing_realtime_url_lookup",
      R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data: "The main frame URL that did not match the local safelist."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing real time URL checks by "
            "unchecking 'Protect you and your device from dangerous sites' in "
            "Chromium settings under Privacy, or by unchecking 'Make searches "
            "and browsing better (Sends URLs of pages you visit to Google)' in "
            "Chromium settings under Privacy."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");
}

base::Optional<std::string> RealTimeUrlLookupService::GetDMTokenString() const {
  // DM token should only be set for enterprise requests.
  return base::nullopt;
}

std::string RealTimeUrlLookupService::GetMetricSuffix() const {
  return ".Consumer";
}

bool RealTimeUrlLookupService::ShouldIncludeCredentials() const {
  return true;
}

}  // namespace safe_browsing
