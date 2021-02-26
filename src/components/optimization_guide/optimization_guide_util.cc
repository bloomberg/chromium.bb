// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_util.h"

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/variations/active_field_trials.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace optimization_guide {

std::string GetStringNameForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN:
      return "Unknown";
    case optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:
      return "PainfulPageLoad";
  }
  NOTREACHED();
  return std::string();
}

bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host) {
  if (net::HostStringIsLocalhost(host))
    return false;
  url::CanonHostInfo host_info;
  std::string canonicalized_host(net::CanonicalizeHost(host, &host_info));
  if (host_info.IsIPAddress() ||
      !net::IsCanonicalizedHostCompliant(canonicalized_host)) {
    return false;
  }
  return true;
}

optimization_guide::OptimizationGuideDecision
GetOptimizationGuideDecisionFromOptimizationTypeDecision(
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_type_decision) {
    case optimization_guide::OptimizationTypeDecision::
        kAllowedByOptimizationFilter:
    case optimization_guide::OptimizationTypeDecision::kAllowedByHint:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTypeDecision::kUnknown:
    case optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHadHintButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHintFetchStartedButNotAvailableInTime:
    case optimization_guide::OptimizationTypeDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    case optimization_guide::OptimizationTypeDecision::kNotAllowedByHint:
    case optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint:
    case optimization_guide::OptimizationTypeDecision::kNoHintAvailable:
    case optimization_guide::OptimizationTypeDecision::
        kNotAllowedByOptimizationFilter:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
}

google::protobuf::RepeatedPtrField<proto::FieldTrial>
GetActiveFieldTrialsAllowedForFetch() {
  google::protobuf::RepeatedPtrField<proto::FieldTrial>
      filtered_active_field_trials;

  base::flat_set<uint32_t> allowed_field_trials_for_fetch =
      features::FieldTrialNameHashesAllowedForFetch();
  if (allowed_field_trials_for_fetch.empty())
    return filtered_active_field_trials;

  std::vector<variations::ActiveGroupId> active_field_trials;
  variations::GetFieldTrialActiveGroupIds(/*suffix=*/"", &active_field_trials);
  for (const auto& active_field_trial : active_field_trials) {
    if (static_cast<size_t>(filtered_active_field_trials.size()) ==
        allowed_field_trials_for_fetch.size()) {
      // We've found all the field trials that we are allowed to send to the
      // server.
      break;
    }

    if (allowed_field_trials_for_fetch.find(active_field_trial.name) ==
        allowed_field_trials_for_fetch.end()) {
      // Continue if we are not allowed to send the field trial to the server.
      continue;
    }

    proto::FieldTrial* ft_proto = filtered_active_field_trials.Add();
    ft_proto->set_name_hash(active_field_trial.name);
    ft_proto->set_group_hash(active_field_trial.group);
  }
  return filtered_active_field_trials;
}

}  // namespace optimization_guide
