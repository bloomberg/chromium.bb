// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/force_youtube_safety_mode_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

ForceYouTubeSafetyModePolicyHandler::ForceYouTubeSafetyModePolicyHandler()
    : TypeCheckingPolicyHandler(key::kForceYouTubeSafetyMode,
                                base::Value::Type::BOOLEAN) {}

ForceYouTubeSafetyModePolicyHandler::~ForceYouTubeSafetyModePolicyHandler() =
    default;

void ForceYouTubeSafetyModePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // If only the deprecated ForceYouTubeSafetyMode policy is set,
  // but not ForceYouTubeRestrict, set ForceYouTubeRestrict to Moderate.
  if (policies.GetValue(key::kForceYouTubeRestrict))
    return;

  const base::Value* value = policies.GetValue(policy_name());
  bool enabled;
  if (value && value->GetAsBoolean(&enabled)) {
    prefs->SetValue(
        prefs::kForceYouTubeRestrict,
        base::Value(enabled ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                            : safe_search_util::YOUTUBE_RESTRICT_OFF));
  }
}

}  // namespace policy
