// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager {
namespace features {

// The feature that gates whether or not the PM runs on the main (UI) thread.
extern const base::Feature kRunOnMainThread;

#if !defined(OS_ANDROID)
// Enables urgent discarding of pages directly from PerformanceManager rather
// than via TabManager.
extern const base::Feature kUrgentDiscardingFromPerformanceManager;

// The discard strategy to use.
// Integer values are specified to allow conversion from the integer value in
// the DiscardStrategy feature param.
enum class DiscardStrategy : int {
  // Discards the least recently used tab among the eligible ones. This is the
  // default strategy.
  LRU = 0,
  // Discard the tab with the biggest resident set among the eligible ones.
  BIGGEST_RSS = 1,
};

class UrgentDiscardingParams {
 public:
  ~UrgentDiscardingParams();

  static UrgentDiscardingParams GetParams();

  DiscardStrategy discard_strategy() const { return discard_strategy_; }

  static constexpr base::FeatureParam<int> kDiscardStrategy{
      &features::kUrgentDiscardingFromPerformanceManager, "DiscardStrategy",
      static_cast<int>(DiscardStrategy::LRU)};

 private:
  UrgentDiscardingParams();
  UrgentDiscardingParams(const UrgentDiscardingParams& rhs);

  DiscardStrategy discard_strategy_;
};

// Feature that controls whether or not tabs should be automatically discarded
// when the total PMF is too high.
extern const base::Feature kHighPMFDiscardPolicy;

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
extern const base::Feature kBackgroundTabLoadingFromPerformanceManager;
#endif

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
extern const base::Feature kBFCachePerformanceManagerPolicy;

// Parameters allowing to control some aspects of the
// |kBFCachePerformanceManagerPolicy|.
class BFCachePerformanceManagerPolicyParams {
 public:
  BFCachePerformanceManagerPolicyParams(
      BFCachePerformanceManagerPolicyParams&&) = default;
  BFCachePerformanceManagerPolicyParams& operator=(
      BFCachePerformanceManagerPolicyParams&&) = default;
  BFCachePerformanceManagerPolicyParams(
      const BFCachePerformanceManagerPolicyParams&) = delete;
  BFCachePerformanceManagerPolicyParams& operator=(
      const BFCachePerformanceManagerPolicyParams&) = delete;
  ~BFCachePerformanceManagerPolicyParams() = default;

  static BFCachePerformanceManagerPolicyParams GetParams();

  // Whether or not the BFCache of all pages should be flushed when the system
  // is under *moderate* memory pressure. The policy always flushes the bfcache
  // under critical pressure.
  bool flush_on_moderate_pressure() const {
    return flush_on_moderate_pressure_;
  }

  base::TimeDelta delay_to_flush_background_tab() const {
    return delay_to_flush_background_tab_;
  }

  static constexpr base::FeatureParam<bool> kFlushOnModeratePressure{
      &features::kBFCachePerformanceManagerPolicy, "flush_on_moderate_pressure",
      false};

  // The back forward cache should be flushed after the tab goes to background
  // and elapses this delay. If the value is negative (such as -1), the back
  // forward cache in the background tabs will not be flushed.
  static constexpr base::FeatureParam<int> kDelayToFlushBackgroundTabInSeconds{
      &features::kBFCachePerformanceManagerPolicy,
      "delay_to_flush_background_tab_in_seconds", -1};

 private:
  BFCachePerformanceManagerPolicyParams() = default;

  bool flush_on_moderate_pressure_;
  base::TimeDelta delay_to_flush_background_tab_;
};

}  // namespace features
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
