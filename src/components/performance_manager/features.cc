// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace performance_manager {
namespace features {

const base::Feature kRunOnMainThread{"RunOnMainThread",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
const base::Feature kUrgentDiscardingFromPerformanceManager {
  "UrgentDiscardingFromPerformanceManager",
// Ash Chrome uses memory pressure evaluator instead of performance manager to
// discard tabs.
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

UrgentDiscardingParams::UrgentDiscardingParams() = default;
UrgentDiscardingParams::UrgentDiscardingParams(
    const UrgentDiscardingParams& rhs) = default;
UrgentDiscardingParams::~UrgentDiscardingParams() = default;

constexpr base::FeatureParam<int> UrgentDiscardingParams::kDiscardStrategy;

// static
UrgentDiscardingParams UrgentDiscardingParams::GetParams() {
  UrgentDiscardingParams params = {};
  params.discard_strategy_ = static_cast<DiscardStrategy>(
      UrgentDiscardingParams::kDiscardStrategy.Get());
  return params;
}

const base::Feature kBackgroundTabLoadingFromPerformanceManager{
    "BackgroundTabLoadingFromPerformanceManager",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHighPMFDiscardPolicy{"HighPMFDiscardPolicy",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kBFCachePerformanceManagerPolicy{
    "BFCachePerformanceManagerPolicy", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool>
    BFCachePerformanceManagerPolicyParams::kFlushOnModeratePressure;

constexpr base::FeatureParam<int>
    BFCachePerformanceManagerPolicyParams::kDelayToFlushBackgroundTabInSeconds;

// static
BFCachePerformanceManagerPolicyParams
BFCachePerformanceManagerPolicyParams::GetParams() {
  BFCachePerformanceManagerPolicyParams params;
  params.flush_on_moderate_pressure_ =
      BFCachePerformanceManagerPolicyParams::kFlushOnModeratePressure.Get();
  params.delay_to_flush_background_tab_ = base::Seconds(
      BFCachePerformanceManagerPolicyParams::kDelayToFlushBackgroundTabInSeconds
          .Get());
  return params;
}

}  // namespace features
}  // namespace performance_manager
