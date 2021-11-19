// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/features.h"

#include "base/command_line.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace scheduler {

namespace {

enum class PolicyOverride { kNoOverride, kForceDisable, kForceEnable };

bool g_intensive_wake_up_throttling_policy_override_cached_ = false;

// Returns the IntensiveWakeUpThrottling policy settings. This is checked once
// on first access and cached. Note that that this is *not* thread-safe!
PolicyOverride GetIntensiveWakeUpThrottlingPolicyOverride() {
  static PolicyOverride policy = PolicyOverride::kNoOverride;
  if (g_intensive_wake_up_throttling_policy_override_cached_)
    return policy;

  // Otherwise, check the command-line. Only values of "0" and "1" are valid,
  // anything else is ignored (and allows the base::Feature to control the
  // feature). This slow path will only be hit once per renderer process.
  g_intensive_wake_up_throttling_policy_override_cached_ = true;
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIntensiveWakeUpThrottlingPolicy);
  if (value == switches::kIntensiveWakeUpThrottlingPolicy_ForceEnable) {
    policy = PolicyOverride::kForceEnable;
  } else if (value == switches::kIntensiveWakeUpThrottlingPolicy_ForceDisable) {
    policy = PolicyOverride::kForceDisable;
  } else {
    // Necessary in testing configurations, as the policy can be parsed
    // repeatedly.
    policy = PolicyOverride::kNoOverride;
  }

  return policy;
}

}  // namespace

void ClearIntensiveWakeUpThrottlingPolicyOverrideCacheForTesting() {
  g_intensive_wake_up_throttling_policy_override_cached_ = false;
}

bool IsIntensiveWakeUpThrottlingEnabled() {
  // If policy is present then respect it.
  auto policy = GetIntensiveWakeUpThrottlingPolicyOverride();
  if (policy != PolicyOverride::kNoOverride)
    return policy == PolicyOverride::kForceEnable;
  // Otherwise respect the base::Feature.
  return base::FeatureList::IsEnabled(features::kIntensiveWakeUpThrottling);
}

// If a policy override is specified then stick to the published defaults so
// that admins get consistent behaviour that clients can't override. Otherwise
// use the base::FeatureParams.

base::TimeDelta GetIntensiveWakeUpThrottlingGracePeriod() {
  // Controls the time that elapses after a page is backgrounded before the
  // throttling policy takes effect.
  static const base::FeatureParam<int>
      kIntensiveWakeUpThrottling_GracePeriodSeconds{
          &features::kIntensiveWakeUpThrottling,
          features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
          kIntensiveWakeUpThrottling_GracePeriodSeconds_Default};

  int seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds_Default;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::kNoOverride) {
    seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds.Get();
  }
  return base::Seconds(seconds);
}

const base::Feature kThrottleForegroundTimers{
    "ThrottleForegroundTimers", base::FEATURE_DISABLED_BY_DEFAULT};

base::TimeDelta GetForegroundTimersThrottledWakeUpInterval() {
  constexpr int kForegroundTimersThrottling_WakeUpIntervalMillis_Default = 100;
  static const base::FeatureParam<int>
      kForegroundTimersThrottledWakeUpIntervalMills{
          &kThrottleForegroundTimers,
          "ForegroundTimersThrottledWakeUpIntervalMills",
          kForegroundTimersThrottling_WakeUpIntervalMillis_Default};
  return base::Milliseconds(
      kForegroundTimersThrottledWakeUpIntervalMills.Get());
}

const base::Feature kDeprioritizeDOMTimersDuringPageLoading{
    "DeprioritizeDOMTimersDuringPageLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<DeprioritizeDOMTimersPhase>::Option
    kDeprioritizeDOMTimersPhaseOptions[] = {
        {DeprioritizeDOMTimersPhase::kOnDOMContentLoaded, "ondomcontentloaded"},
        {DeprioritizeDOMTimersPhase::kFirstContentfulPaint, "fcp"},
        {DeprioritizeDOMTimersPhase::kOnLoad, "onload"}};

const base::FeatureParam<DeprioritizeDOMTimersPhase>
    kDeprioritizeDOMTimersPhase{&kDeprioritizeDOMTimersDuringPageLoading,
                                "phase",
                                DeprioritizeDOMTimersPhase::kOnDOMContentLoaded,
                                &kDeprioritizeDOMTimersPhaseOptions};
}  // namespace scheduler
}  // namespace blink
