// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_

#include "base/traits_bag.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// A list of things a feature can opt out from on the behalf of the page
// if the page is using this feature.
// See FrameOrWorkerScheduler::RegisterFeature.
struct PLATFORM_EXPORT SchedulingPolicy {
  // List of features which can trigger the policy changes.
  enum class Feature {
    kWebSocket = 0,
    kWebRTC = 1,

    kCount = 2
  };

  // List of opt-outs which form a policy.
  struct DisableAggressiveThrottling {};

  struct ValidPolicies {
    ValidPolicies(DisableAggressiveThrottling);
  };

  template <class... ArgTypes,
            class CheckArgumentsAreValid = std::enable_if_t<
                base::trait_helpers::AreValidTraits<ValidPolicies,
                                                    ArgTypes...>::value>>
  constexpr SchedulingPolicy(ArgTypes... args)
      : disable_aggressive_throttling(
            base::trait_helpers::HasTrait<DisableAggressiveThrottling>(
                args...)) {}

  SchedulingPolicy() {}

  bool disable_aggressive_throttling = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_
