// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_fetcher.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace optimization_guide {

namespace {

// The duration that hosts placed in the HintsFetcherHostsSuccessfullyFetched
// dictionary pref are considered valid.
constexpr base::TimeDelta kHintsFetcherHostFetchedValidDuration =
    base::TimeDelta::FromDays(7);

}  // namespace

HintsFetcher::HintsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL optimization_guide_service_url,
    PrefService* pref_service)
    : optimization_guide_service_url_(net::AppendOrReplaceQueryParameter(
          optimization_guide_service_url,
          "key",
          features::GetOptimizationGuideServiceAPIKey())),
      pref_service_(pref_service),
      time_clock_(base::DefaultClock::GetInstance()) {
  url_loader_factory_ = std::move(url_loader_factory);
  CHECK(optimization_guide_service_url_.SchemeIs(url::kHttpsScheme));
  CHECK(features::IsHintsFetchingEnabled());
}

HintsFetcher::~HintsFetcher() {}

// static
void HintsFetcher::ClearHostsSuccessfullyFetched(PrefService* pref_service) {
  DictionaryPrefUpdate hosts_fetched_list(
      pref_service, prefs::kHintsFetcherHostsSuccessfullyFetched);
  hosts_fetched_list->Clear();
}

void HintsFetcher::SetTimeClockForTesting(const base::Clock* time_clock) {
  time_clock_ = time_clock;
}

// static
void HintsFetcher::RecordHintsFetcherCoverage(PrefService* pref_service,
                                              const std::string& host) {
  DictionaryPrefUpdate hosts_fetched(
      pref_service, prefs::kHintsFetcherHostsSuccessfullyFetched);
  base::Optional<double> value =
      hosts_fetched->FindDoubleKey(HashHostForDictionary(host));
  if (!value) {
    UMA_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", false);
    return;
  }

  base::Time host_valid_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromSecondsD(*value));
  UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.HintsFetcher.WasHostCoveredByFetch",
                        host_valid_time > base::Time::Now());
}

bool HintsFetcher::FetchOptimizationGuideServiceHints(
    const std::vector<std::string>& hosts,
    HintsFetchedCallback hints_fetched_callback) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    std::move(hints_fetched_callback).Run(base::nullopt);
    return false;
  }

  if (url_loader_)
    return false;

  get_hints_request_ = std::make_unique<proto::GetHintsRequest>();

  // Add all the optimizations supported by the current version of Chrome,
  // regardless of whether the session is in holdback for any of them.
  get_hints_request_->add_supported_optimizations(proto::NOSCRIPT);
  get_hints_request_->add_supported_optimizations(proto::RESOURCE_LOADING);
  get_hints_request_->add_supported_optimizations(proto::DEFER_ALL_SCRIPT);
  // TODO(crbug/969558): Figure out a way to either have a registration call
  // for clients to specify their supported optimization types or have a static
  // assert on the last OptimizationType.

  get_hints_request_->set_context(proto::CONTEXT_BATCH_UPDATE);

  for (const auto& host : hosts) {
    proto::HostInfo* host_info = get_hints_request_->add_hosts();
    host_info->set_host(host);
  }

  std::string serialized_request;
  get_hints_request_->SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("hintsfetcher_gethintsrequest", R"(
        semantics {
          sender: "HintsFetcher"
          description:
            "Requests Hints from the Optimization Guide Service for use in "
            "providing data saving and pageload optimizations for Chrome."
          trigger:
            "Requested periodically if Data Saver is enabled and the browser "
            "has Hints that are older than a threshold set by "
            "the server."
          data: "A list of the user's most engaged websites."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Data Saver on Android via 'Data Saver' setting. "
            "Data Saver is not available on iOS."
          policy_exception_justification: "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = optimization_guide_service_url_;

  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_PROXY;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/x-protobuf");

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", hosts.size());

  // |url_loader_| should not retry on 5xx errors since the server may already
  // be overloaded.  |url_loader_| should retry on network changes since the
  // network stack may receive the connection change event later than |this|.
  static const int kMaxRetries = 1;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&HintsFetcher::OnURLLoadComplete, base::Unretained(this)));

  hints_fetched_callback_ = std::move(hints_fetched_callback);
  hosts_fetched_ = hosts;
  return true;
}

void HintsFetcher::HandleResponse(const std::string& get_hints_response_data,
                                  int net_status,
                                  int response_code) {
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  UMA_HISTOGRAM_ENUMERATION(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status",
      static_cast<net::HttpStatusCode>(response_code),
      net::HTTP_VERSION_NOT_SUPPORTED);
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode",
      -net_status);

  if (net_status == net::OK && response_code == net::HTTP_OK &&
      get_hints_response->ParseFromString(get_hints_response_data)) {
    UMA_HISTOGRAM_COUNTS_100(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount",
        get_hints_response->hints_size());
    UpdateHostsSuccessfullyFetched();
    std::move(hints_fetched_callback_).Run(std::move(get_hints_response));
  } else {
    std::move(hints_fetched_callback_).Run(base::nullopt);
  }
}

void HintsFetcher::UpdateHostsSuccessfullyFetched() {
  DictionaryPrefUpdate hosts_fetched_list(
      pref_service_, prefs::kHintsFetcherHostsSuccessfullyFetched);

  // Remove any expired hosts.
  std::vector<std::string> entries_to_remove;
  for (const auto& it : hosts_fetched_list->DictItems()) {
    if (base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromSecondsD(
            it.second.GetDouble())) < time_clock_->Now()) {
      entries_to_remove.emplace_back(it.first);
    }
  }
  for (const auto& host : entries_to_remove) {
    hosts_fetched_list->Remove(host, nullptr);
  }

  if (hosts_fetched_.empty())
    return;

  // Ensure there is enough space in the dictionary pref for the
  // most recent set of hosts to be stored.
  if (hosts_fetched_list->size() + hosts_fetched_.size() >
      features::MaxHostsForRecordingSuccessfullyCovered()) {
    entries_to_remove.clear();
    size_t num_entries_to_remove =
        hosts_fetched_list->size() + hosts_fetched_.size() -
        features::MaxHostsForRecordingSuccessfullyCovered();
    for (const auto& it : hosts_fetched_list->DictItems()) {
      if (entries_to_remove.size() >= num_entries_to_remove)
        break;
      entries_to_remove.emplace_back(it.first);
    }
    for (const auto& host : entries_to_remove) {
      hosts_fetched_list->Remove(host, nullptr);
    }
  }

  // Add the covered hosts in |hosts_fetched_| to the dictionary pref.
  base::Time host_invalid_time =
      time_clock_->Now() + kHintsFetcherHostFetchedValidDuration;
  for (const std::string& host : hosts_fetched_) {
    hosts_fetched_list->SetDoubleKey(
        HashHostForDictionary(host),
        host_invalid_time.ToDeltaSinceWindowsEpoch().InSecondsF());
  }
  DCHECK_LE(hosts_fetched_list->size(),
            features::MaxHostsForRecordingSuccessfullyCovered());
  hosts_fetched_.clear();
}

void HintsFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  HandleResponse(response_body ? *response_body : "", url_loader_->NetError(),
                 response_code);
  url_loader_.reset();
}

}  // namespace optimization_guide
