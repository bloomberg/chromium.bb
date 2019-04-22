// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler_params_manager.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

// The maximum number of delayable requests to allow to be in-flight at any
// point in time (across all hosts).
constexpr size_t kDefaultMaxNumDelayableRequestsPerClient = 10;

// Value by which HTTP RTT estimate is multiplied to get the maximum queuing
// duration.
constexpr int kHttpRttMultiplierForQueuingDuration = 30;

// Reads experiment parameters and returns them.
ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer
GetParamsForNetworkQualityContainer() {
  // Look for configuration parameters with sequential numeric suffixes, and
  // stop looking after the first failure to find an experimetal parameter.
  // A sample configuration is given below:
  // "EffectiveConnectionType1": "Slow-2G",
  // "MaxDelayableRequests1": "6",
  // "NonDelayableWeight1": "2.0",
  // "EffectiveConnectionType2": "3G",
  // "MaxDelayableRequests2": "12",
  // "NonDelayableWeight2": "3.0",
  // This config implies that when Effective Connection Type (ECT) is Slow-2G,
  // then the maximum number of non-delayable requests should be
  // limited to 6, and the non-delayable request weight should be set to 2.
  // When ECT is 3G, it should be limited to 12. For all other values of ECT,
  // the default values are used.
  static const char kMaxDelayableRequestsBase[] = "MaxDelayableRequests";
  static const char kEffectiveConnectionTypeBase[] = "EffectiveConnectionType";
  static const char kNonDelayableWeightBase[] = "NonDelayableWeight";
  static constexpr base::TimeDelta kUpperBoundQueuingDuration =
      base::TimeDelta::FromSeconds(120);
  static constexpr base::TimeDelta kLowerBoundQueuingDuration =
      base::TimeDelta::FromSeconds(15);

  ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer result;
  // Set the default params for networks with ECT Slow2G and 2G. These params
  // can still be overridden using the field trial.
  result.emplace(
      std::make_pair(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                     ResourceSchedulerParamsManager::ParamsForNetworkQuality(
                         8, 3.0, false, base::nullopt)));
  result.emplace(
      std::make_pair(net::EFFECTIVE_CONNECTION_TYPE_2G,
                     ResourceSchedulerParamsManager::ParamsForNetworkQuality(
                         8, 3.0, false, base::nullopt)));

  for (int config_param_index = 1; config_param_index <= 20;
       ++config_param_index) {
    size_t max_delayable_requests;

    if (!base::StringToSizeT(base::GetFieldTrialParamValueByFeature(
                                 features::kThrottleDelayable,
                                 kMaxDelayableRequestsBase +
                                     base::NumberToString(config_param_index)),
                             &max_delayable_requests)) {
      break;
    }

    base::Optional<net::EffectiveConnectionType> effective_connection_type =
        net::GetEffectiveConnectionTypeForName(
            base::GetFieldTrialParamValueByFeature(
                features::kThrottleDelayable,
                kEffectiveConnectionTypeBase +
                    base::NumberToString(config_param_index)));
    DCHECK(effective_connection_type.has_value());

    double non_delayable_weight = base::GetFieldTrialParamByFeatureAsDouble(
        features::kThrottleDelayable,
        kNonDelayableWeightBase + base::NumberToString(config_param_index),
        0.0);

    // Check if the entry is already present. This will happen if the default
    // params are being overridden by the field trial.
    ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer::iterator
        iter = result.find(effective_connection_type.value());
    if (iter != result.end()) {
      iter->second.max_delayable_requests = max_delayable_requests;
      iter->second.non_delayable_weight = non_delayable_weight;
    } else {
      result.emplace(std::make_pair(
          effective_connection_type.value(),
          ResourceSchedulerParamsManager::ParamsForNetworkQuality(
              max_delayable_requests, non_delayable_weight, false,
              base::nullopt)));
    }
  }

  // Next, read the experiments params for
  // DelayRequestsOnMultiplexedConnections finch experiment, and modify |result|
  // based on the experiment params.
  if (base::FeatureList::IsEnabled(
          features::kDelayRequestsOnMultiplexedConnections)) {
    base::Optional<net::EffectiveConnectionType> max_effective_connection_type =
        net::GetEffectiveConnectionTypeForName(
            base::GetFieldTrialParamValueByFeature(
                features::kDelayRequestsOnMultiplexedConnections,
                "MaxEffectiveConnectionType"));

    if (!max_effective_connection_type) {
      // Use a default value if one is not set using field trial params.
      max_effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_3G;
    }

    for (int ect = net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
         ect <= max_effective_connection_type.value(); ++ect) {
      net::EffectiveConnectionType effective_connection_type =
          static_cast<net::EffectiveConnectionType>(ect);
      ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer::iterator
          iter = result.find(effective_connection_type);
      if (iter != result.end()) {
        iter->second.delay_requests_on_multiplexed_connections = true;
      } else {
        result.emplace(std::make_pair(
            effective_connection_type,
            ResourceSchedulerParamsManager::ParamsForNetworkQuality(
                kDefaultMaxNumDelayableRequestsPerClient, 0.0, true,
                base::nullopt)));
      }
    }
  }

  for (int ect = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       ect <= net::EFFECTIVE_CONNECTION_TYPE_4G; ++ect) {
    net::EffectiveConnectionType effective_connection_type =
        static_cast<net::EffectiveConnectionType>(ect);
    base::TimeDelta http_rtt =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
            effective_connection_type);
    base::TimeDelta max_queuing_time =
        http_rtt * kHttpRttMultiplierForQueuingDuration;

    // If GetDefaultTypicalHttpRtt returns a null value, set
    // |max_queuing_time| to kUpperBoundQueuingDuration, This may happen
    // when |ect| is UNKNOWN or OFFLINE. Both these cases are very rare, but
    // it's important to handle them to ensure that |max_queuing_time|
    // is set to some non-zero value in all cases. This ensures that the
    // requests that are queued for too long are always unthrottled.
    if (http_rtt.is_zero())
      max_queuing_time = kUpperBoundQueuingDuration;
    if (max_queuing_time < kLowerBoundQueuingDuration)
      max_queuing_time = kLowerBoundQueuingDuration;
    if (max_queuing_time > kUpperBoundQueuingDuration)
      max_queuing_time = kUpperBoundQueuingDuration;

    ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer::iterator
        iter = result.find(effective_connection_type);
    if (iter != result.end()) {
      iter->second.max_queuing_time = max_queuing_time;
    } else {
      result.emplace(std::make_pair(
          effective_connection_type,
          ResourceSchedulerParamsManager::ParamsForNetworkQuality(
              kDefaultMaxNumDelayableRequestsPerClient, 0.0, false,
              max_queuing_time)));
    }
  }

  return result;
}

}  // namespace

ResourceSchedulerParamsManager::ParamsForNetworkQuality::
    ParamsForNetworkQuality()
    : ResourceSchedulerParamsManager::ParamsForNetworkQuality(
          kDefaultMaxNumDelayableRequestsPerClient,
          0.0,
          false,
          base::nullopt) {}

ResourceSchedulerParamsManager::ParamsForNetworkQuality::
    ParamsForNetworkQuality(size_t max_delayable_requests,
                            double non_delayable_weight,
                            bool delay_requests_on_multiplexed_connections,
                            base::Optional<base::TimeDelta> max_queuing_time)
    : max_delayable_requests(max_delayable_requests),
      non_delayable_weight(non_delayable_weight),
      delay_requests_on_multiplexed_connections(
          delay_requests_on_multiplexed_connections),
      max_queuing_time(max_queuing_time) {}

ResourceSchedulerParamsManager::ParamsForNetworkQuality::
    ParamsForNetworkQuality(
        const ResourceSchedulerParamsManager::ParamsForNetworkQuality& other) =
        default;

ResourceSchedulerParamsManager::ResourceSchedulerParamsManager()
    : ResourceSchedulerParamsManager(GetParamsForNetworkQualityContainer()) {}

ResourceSchedulerParamsManager::ResourceSchedulerParamsManager(
    const ParamsForNetworkQualityContainer&
        params_for_network_quality_container)
    : params_for_network_quality_container_(
          params_for_network_quality_container) {}

ResourceSchedulerParamsManager::ResourceSchedulerParamsManager(
    const ResourceSchedulerParamsManager& other)
    : params_for_network_quality_container_(
          other.params_for_network_quality_container_) {}

ResourceSchedulerParamsManager::~ResourceSchedulerParamsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ResourceSchedulerParamsManager::ParamsForNetworkQuality
ResourceSchedulerParamsManager::GetParamsForEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ParamsForNetworkQualityContainer::const_iterator iter =
      params_for_network_quality_container_.find(effective_connection_type);
  if (iter != params_for_network_quality_container_.end())
    return iter->second;
  return ParamsForNetworkQuality(kDefaultMaxNumDelayableRequestsPerClient, 0.0,
                                 false, base::nullopt);
}

}  // namespace network
