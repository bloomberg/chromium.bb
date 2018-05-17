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

namespace {

// The maximum number of delayable requests to allow to be in-flight at any
// point in time (across all hosts).
static const size_t kDefaultMaxNumDelayableRequestsPerClient = 10;

// Based on the field trial parameters, this feature will override the value of
// the maximum number of delayable requests allowed in flight. The number of
// delayable requests allowed in flight will be based on the network's
// effective connection type ranges and the
// corresponding number of delayable requests in flight specified in the
// experiment configuration. Based on field trial parameters, this experiment
// may also throttle delayable requests based on the number of non-delayable
// requests in-flight times a weighting factor.
const base::Feature kThrottleDelayable{"ThrottleDelayable",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace

namespace network {

ResourceSchedulerParamsManager::ParamsForNetworkQuality::
    ParamsForNetworkQuality()
    : max_delayable_requests(kDefaultMaxNumDelayableRequestsPerClient),
      non_delayable_weight(0.0) {}

ResourceSchedulerParamsManager::ParamsForNetworkQuality::
    ParamsForNetworkQuality(size_t max_delayable_requests,
                            double non_delayable_weight)
    : max_delayable_requests(max_delayable_requests),
      non_delayable_weight(non_delayable_weight) {}

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
  return ParamsForNetworkQuality(kDefaultMaxNumDelayableRequestsPerClient, 0.0);
}

// static
ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer
ResourceSchedulerParamsManager::GetParamsForNetworkQualityContainer() {
  static const char kMaxDelayableRequestsBase[] = "MaxDelayableRequests";
  static const char kEffectiveConnectionTypeBase[] = "EffectiveConnectionType";
  static const char kNonDelayableWeightBase[] = "NonDelayableWeight";

  ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer result;
  // Set the default params for networks with ECT Slow2G and 2G. These params
  // can still be overridden using the field trial.
  result.emplace(std::make_pair(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                                ParamsForNetworkQuality(8, 3.0)));
  result.emplace(std::make_pair(net::EFFECTIVE_CONNECTION_TYPE_2G,
                                ParamsForNetworkQuality(8, 3.0)));

  for (int config_param_index = 1; config_param_index <= 20;
       ++config_param_index) {
    size_t max_delayable_requests;

    if (!base::StringToSizeT(
            base::GetFieldTrialParamValueByFeature(
                kThrottleDelayable, kMaxDelayableRequestsBase +
                                        base::IntToString(config_param_index)),
            &max_delayable_requests)) {
      return result;
    }

    base::Optional<net::EffectiveConnectionType> effective_connection_type =
        net::GetEffectiveConnectionTypeForName(
            base::GetFieldTrialParamValueByFeature(
                kThrottleDelayable, kEffectiveConnectionTypeBase +
                                        base::IntToString(config_param_index)));
    DCHECK(effective_connection_type.has_value());

    double non_delayable_weight = base::GetFieldTrialParamByFeatureAsDouble(
        kThrottleDelayable,
        kNonDelayableWeightBase + base::IntToString(config_param_index), 0.0);

    // Check if the entry is already present. This will happen if the default
    // params are being overridden by the field trial.
    ParamsForNetworkQualityContainer::iterator iter =
        result.find(effective_connection_type.value());
    if (iter != result.end()) {
      iter->second.max_delayable_requests = max_delayable_requests;
      iter->second.non_delayable_weight = non_delayable_weight;
    } else {
      result.emplace(
          std::make_pair(effective_connection_type.value(),
                         ParamsForNetworkQuality(max_delayable_requests,
                                                 non_delayable_weight)));
    }
  }
  // There should not have been more than 20 params indices specified.
  NOTREACHED();
  return result;
}

}  // namespace network
