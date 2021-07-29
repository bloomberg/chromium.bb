// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace safe_browsing {

namespace {

const size_t kMaxFailuresToEnforceBackoff = 3;

const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

const size_t kURLLookupTimeoutDurationInSeconds = 3;

// Represents the value stored in the |version| field of |RTLookupRequest|.
const int kRTLookupRequestVersion = 2;

// UMA helper functions.
void RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                       const std::string& suffix,
                                       bool value) {
  base::UmaHistogramBoolean(metric, value);
  base::UmaHistogramBoolean(metric + suffix, value);
}

void RecordSparseWithAndWithoutSuffix(const std::string& metric,
                                      const std::string& suffix,
                                      int32_t value) {
  base::UmaHistogramSparse(metric, value);
  base::UmaHistogramSparse(metric + suffix, value);
}

void RecordTimesWithAndWithoutSuffix(const std::string& metric,
                                     const std::string& suffix,
                                     base::TimeDelta value) {
  base::UmaHistogramTimes(metric, value);
  base::UmaHistogramTimes(metric + suffix, value);
}

void RecordCount100WithAndWithoutSuffix(const std::string& metric,
                                        const std::string& suffix,
                                        int value) {
  base::UmaHistogramCounts100(metric, value);
  base::UmaHistogramCounts100(metric + suffix, value);
}

void RecordCount1MWithAndWithoutSuffix(const std::string& metric,
                                       const std::string& suffix,
                                       int value) {
  base::UmaHistogramCounts1M(metric, value);
  base::UmaHistogramCounts1M(metric + suffix, value);
}

void RecordRequestPopulationWithAndWithoutSuffix(
    const std::string& metric,
    const std::string& suffix,
    ChromeUserPopulation::UserPopulation population) {
  base::UmaHistogramExactLinear(metric, population,
                                ChromeUserPopulation::UserPopulation_MAX + 1);
  base::UmaHistogramExactLinear(metric + suffix, population,
                                ChromeUserPopulation::UserPopulation_MAX + 1);
}

void RecordNetworkResultWithAndWithoutSuffix(const std::string& metric,
                                             const std::string& suffix,
                                             int net_error,
                                             int response_code) {
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      metric.c_str(), net_error, response_code);
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      (metric + suffix).c_str(), net_error, response_code);
}

RTLookupRequest::OSType GetRTLookupRequestOSType() {
#if defined(OS_ANDROID)
  return RTLookupRequest::OS_TYPE_ANDROID;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return RTLookupRequest::OS_TYPE_CHROME_OS;
#elif defined(OS_FUCHSIA)
  return RTLookupRequest::OS_TYPE_FUCHSIA;
#elif defined(OS_IOS)
  return RTLookupRequest::OS_TYPE_IOS;
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return RTLookupRequest::OS_TYPE_LINUX;
#elif defined(OS_MAC)
  return RTLookupRequest::OS_TYPE_MAC;
#elif defined(OS_WIN)
  return RTLookupRequest::OS_TYPE_WINDOWS;
#else
  return RTLookupRequest::OS_TYPE_UNSPECIFIED;
#endif
}

}  // namespace

RealTimeUrlLookupServiceBase::RealTimeUrlLookupServiceBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    ReferrerChainProvider* referrer_chain_provider)
    : url_loader_factory_(url_loader_factory),
      cache_manager_(cache_manager),
      get_user_population_callback_(get_user_population_callback),
      referrer_chain_provider_(referrer_chain_provider) {}

RealTimeUrlLookupServiceBase::~RealTimeUrlLookupServiceBase() = default;

// static
bool RealTimeUrlLookupServiceBase::CanCheckUrl(const GURL& url) {
  if (VerdictCacheManager::has_artificial_unsafe_url()) {
    return true;
  }
  return CanGetReputationOfUrl(url);
}

// static
SBThreatType RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
    RTLookupResponse::ThreatInfo::ThreatType rt_threat_type) {
  switch (rt_threat_type) {
    case RTLookupResponse::ThreatInfo::WEB_MALWARE:
      return SB_THREAT_TYPE_URL_MALWARE;
    case RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case RTLookupResponse::ThreatInfo::UNCLEAR_BILLING:
      return SB_THREAT_TYPE_BILLING;
    case RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unexpected RTLookupResponse::ThreatType encountered";
      return SB_THREAT_TYPE_SAFE;
  }
}

// static
GURL RealTimeUrlLookupServiceBase::SanitizeURL(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

// static
void RealTimeUrlLookupServiceBase::SanitizeReferrerChainEntries(
    ReferrerChain* referrer_chain) {
  for (ReferrerChainEntry& entry : *referrer_chain) {
    // is_subframe_url_removed is added in the proto.
    // If the entry sets main_frame_url, that means the url is triggered in a
    // subframe. Thus replace the url with the main_frame_url and clear
    // the main_frame_url field.
    if (entry.has_main_frame_url()) {
      entry.set_url(entry.main_frame_url());
      entry.clear_main_frame_url();
      entry.set_is_subframe_url_removed(true);
    }
    // If the entry sets referrer_main_frame_url, that means the referrer_url is
    // triggered in a subframe. Thus replace the referrer_url with the
    // referrer_main_frame_url and clear the referrer_main_frame_url field.
    if (entry.has_referrer_main_frame_url()) {
      entry.set_referrer_url(entry.referrer_main_frame_url());
      entry.clear_referrer_main_frame_url();
      entry.set_is_subframe_referrer_url_removed(true);
    }
  }
}

base::WeakPtr<RealTimeUrlLookupServiceBase>
RealTimeUrlLookupServiceBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

size_t RealTimeUrlLookupServiceBase::GetBackoffDurationInSeconds() const {
  return did_successful_lookup_since_last_backoff_
             ? kMinBackOffResetDurationInSeconds
             : std::min(kMaxBackOffResetDurationInSeconds,
                        2 * next_backoff_duration_secs_);
}

void RealTimeUrlLookupServiceBase::HandleLookupError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consecutive_failures_++;

  // Any successful lookup clears both |consecutive_failures_| as well as
  // |did_successful_lookup_since_last_backoff_|.
  // On a failure, the following happens:
  // 1) if |consecutive_failures_| < |kMaxFailuresToEnforceBackoff|:
  //    Do nothing more.
  // 2) if already in the backoff mode:
  //    Do nothing more. This can happen if we had some outstanding real time
  //    requests in flight when we entered the backoff mode.
  // 3) if |did_successful_lookup_since_last_backoff_| is true:
  //    Enter backoff mode for |kMinBackOffResetDurationInSeconds| seconds.
  // 4) if |did_successful_lookup_since_last_backoff_| is false:
  //    This indicates that we've had |kMaxFailuresToEnforceBackoff| since
  //    exiting the last backoff with no successful lookups since so do an
  //    exponential backoff.

  if (consecutive_failures_ < kMaxFailuresToEnforceBackoff)
    return;

  if (IsInBackoffMode()) {
    return;
  }

  // Enter backoff mode, calculate duration.
  next_backoff_duration_secs_ = GetBackoffDurationInSeconds();
  backoff_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(next_backoff_duration_secs_),
      this, &RealTimeUrlLookupServiceBase::ResetFailures);
  did_successful_lookup_since_last_backoff_ = false;
}

void RealTimeUrlLookupServiceBase::HandleLookupSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetFailures();

  // |did_successful_lookup_since_last_backoff_| is set to true only when we
  // complete a lookup successfully.
  did_successful_lookup_since_last_backoff_ = true;
}

bool RealTimeUrlLookupServiceBase::IsInBackoffMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool in_backoff = backoff_timer_.IsRunning();
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.Backoff.State",
                                    GetMetricSuffix(), in_backoff);
  return in_backoff;
}

void RealTimeUrlLookupServiceBase::ResetFailures() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consecutive_failures_ = 0;
  backoff_timer_.Stop();
}

std::unique_ptr<RTLookupResponse>
RealTimeUrlLookupServiceBase::GetCachedRealTimeUrlVerdict(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<RTLookupResponse::ThreatInfo> cached_threat_info =
      std::make_unique<RTLookupResponse::ThreatInfo>();

  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.HasValidCacheManager",
                                    GetMetricSuffix(), !!cache_manager_);

  base::TimeTicks get_cache_start_time = base::TimeTicks::Now();

  RTLookupResponse::ThreatInfo::VerdictType verdict_type =
      cache_manager_ ? cache_manager_->GetCachedRealTimeUrlVerdict(
                           url, cached_threat_info.get())
                     : RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED;

  RecordSparseWithAndWithoutSuffix("SafeBrowsing.RT.GetCacheResult",
                                   GetMetricSuffix(), verdict_type);
  RecordTimesWithAndWithoutSuffix(
      "SafeBrowsing.RT.GetCache.Time", GetMetricSuffix(),
      base::TimeTicks::Now() - get_cache_start_time);

  if (verdict_type == RTLookupResponse::ThreatInfo::SAFE ||
      verdict_type == RTLookupResponse::ThreatInfo::DANGEROUS) {
    auto cache_response = std::make_unique<RTLookupResponse>();
    RTLookupResponse::ThreatInfo* new_threat_info =
        cache_response->add_threat_info();
    *new_threat_info = *cached_threat_info;
    return cache_response;
  }
  return nullptr;
}

void RealTimeUrlLookupServiceBase::MayBeCacheRealTimeUrlVerdict(
    const GURL& url,
    RTLookupResponse response) {
  if (response.threat_info_size() > 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&VerdictCacheManager::CacheRealTimeUrlVerdict,
                                  base::Unretained(cache_manager_), url,
                                  response, base::Time::Now()));
  }
}

void RealTimeUrlLookupServiceBase::StartLookup(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  // Check cache.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  if (cache_response) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_callback),
                                  /* is_rt_lookup_successful */ true,
                                  /* is_cached_response */ true,
                                  std::move(cache_response)));
    return;
  }

  if (CanPerformFullURLLookupWithToken()) {
    GetAccessToken(url, std::move(request_callback),
                   std::move(response_callback),
                   std::move(callback_task_runner));
  } else {
    SendRequest(url, /* access_token_string */ absl::nullopt,
                std::move(request_callback), std::move(response_callback),
                std::move(callback_task_runner));
  }
}

void RealTimeUrlLookupServiceBase::SendRequest(
    const GURL& url,
    absl::optional<std::string> access_token_string,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<RTLookupRequest> request = FillRequestProto(url);
  RecordRequestPopulationWithAndWithoutSuffix(
      "SafeBrowsing.RT.Request.UserPopulation", GetMetricSuffix(),
      request->population().user_population());
  RecordCount100WithAndWithoutSuffix(
      "SafeBrowsing.RT.Request.ReferrerChainLength", GetMetricSuffix(),
      request->referrer_chain().size());
  std::string req_data;
  request->SerializeToString(&req_data);

  auto resource_request = GetResourceRequest();
  if (access_token_string.has_value()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({kAuthHeaderBearer, access_token_string.value()}));
  }
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.HasTokenInRequest",
                                    GetMetricSuffix(),
                                    access_token_string.has_value());

  // NOTE: Pass |callback_task_runner| by copying it here as it's also needed
  // just below.
  SendRequestInternal(std::move(resource_request), req_data, url,
                      access_token_string, std::move(response_callback),
                      callback_task_runner);

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(request_callback), std::move(request),
          access_token_string.has_value() ? access_token_string.value() : ""));
}

void RealTimeUrlLookupServiceBase::SendRequestInternal(
    std::unique_ptr<network::ResourceRequest> resource_request,
    const std::string& req_data,
    const GURL& url,
    absl::optional<std::string> access_token_string,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       GetTrafficAnnotationTag());
  network::SimpleURLLoader* loader = owned_loader.get();
  RecordCount1MWithAndWithoutSuffix("SafeBrowsing.RT.Request.Size",
                                    GetMetricSuffix(), req_data.size());
  owned_loader->AttachStringForUpload(req_data, "application/octet-stream");
  owned_loader->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kURLLookupTimeoutDurationInSeconds));
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupServiceBase::OnURLLoaderComplete,
                     GetWeakPtr(), url, access_token_string, loader,
                     base::TimeTicks::Now(), std::move(callback_task_runner)));

  pending_requests_[owned_loader.release()] = std::move(response_callback);
}

void RealTimeUrlLookupServiceBase::OnURLLoaderComplete(
    const GURL& url,
    absl::optional<std::string> access_token_string,
    network::SimpleURLLoader* url_loader,
    base::TimeTicks request_start_time,
    scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_requests_.find(url_loader);
  DCHECK(it != pending_requests_.end()) << "Request not found";

  RecordTimesWithAndWithoutSuffix("SafeBrowsing.RT.Network.Time",
                                  GetMetricSuffix(),
                                  base::TimeTicks::Now() - request_start_time);

  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  RecordNetworkResultWithAndWithoutSuffix("SafeBrowsing.RT.Network.Result",
                                          GetMetricSuffix(), net_error,
                                          response_code);

  if (response_code == net::HTTP_UNAUTHORIZED &&
      access_token_string.has_value()) {
    OnResponseUnauthorized(access_token_string.value());
  }

  auto response = std::make_unique<RTLookupResponse>();
  bool is_rt_lookup_successful = (net_error == net::OK) &&
                                 (response_code == net::HTTP_OK) &&
                                 response->ParseFromString(*response_body);
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.IsLookupSuccessful",
                                    GetMetricSuffix(), is_rt_lookup_successful);
  is_rt_lookup_successful ? HandleLookupSuccess() : HandleLookupError();

  MayBeCacheRealTimeUrlVerdict(url, *response);

  RecordCount100WithAndWithoutSuffix("SafeBrowsing.RT.ThreatInfoSize",
                                     GetMetricSuffix(),
                                     response->threat_info_size());

  response_callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(it->second), is_rt_lookup_successful,
                     /* is_cached_response */ false, std::move(response)));

  delete it->first;
  pending_requests_.erase(it);
}

std::unique_ptr<network::ResourceRequest>
RealTimeUrlLookupServiceBase::GetResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetRealTimeLookupUrl();
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";
  if (!ShouldIncludeCredentials())
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

std::unique_ptr<RTLookupRequest> RealTimeUrlLookupServiceBase::FillRequestProto(
    const GURL& url) {
  auto request = std::make_unique<RTLookupRequest>();
  request->set_url(SanitizeURL(url).spec());
  request->set_lookup_type(RTLookupRequest::NAVIGATION);
  request->set_version(kRTLookupRequestVersion);
  request->set_os_type(GetRTLookupRequestOSType());
  absl::optional<std::string> dm_token_string = GetDMTokenString();
  if (dm_token_string.has_value()) {
    request->set_dm_token(dm_token_string.value());
  }

  *request->mutable_population() = get_user_population_callback_.Run();

  if (CanAttachReferrerChain() && referrer_chain_provider_) {
    referrer_chain_provider_->IdentifyReferrerChainByPendingEventURL(
        SanitizeURL(url), GetReferrerUserGestureLimit(),
        request->mutable_referrer_chain());
    if (!CanCheckSubresourceURL()) {
      SanitizeReferrerChainEntries(request->mutable_referrer_chain());
    }
  }

  return request;
}

void RealTimeUrlLookupServiceBase::OnResponseUnauthorized(
    const std::string& invalid_access_token) {}

void RealTimeUrlLookupServiceBase::Shutdown() {
  for (auto& pending : pending_requests_) {
    // Pending requests are not posted back to the IO thread during shutdown,
    // because it is too late to post a task to the IO thread when the UI thread
    // is shutting down.
    delete pending.first;
  }
  pending_requests_.clear();

  // Clear references to other KeyedServices.
  cache_manager_ = nullptr;
  referrer_chain_provider_ = nullptr;
}

}  // namespace safe_browsing
