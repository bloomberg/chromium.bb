// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"

#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"

namespace predictors {

const char kSpeculativePreconnectFeatureName[] = "SpeculativePreconnect";
const base::Feature kSpeculativePreconnectFeature{
    kSpeculativePreconnectFeatureName, base::FEATURE_ENABLED_BY_DEFAULT};

// Returns whether the speculative preconnect feature is enabled.
bool IsPreconnectFeatureEnabled() {
  return base::FeatureList::IsEnabled(kSpeculativePreconnectFeature);
}

bool IsLoadingPredictorEnabled(Profile* profile) {
  // Disabled for off-the-record. Policy choice, not a technical limitation.
  if (!profile || profile->IsOffTheRecord())
    return false;

  // Run the Loading Predictor only if the preconnect feature is turned on,
  // otherwise the predictor will be no-op.
  return IsPreconnectFeatureEnabled();
}

bool IsPreconnectAllowed(Profile* profile) {
  if (!IsPreconnectFeatureEnabled())
    return false;

  // Checks that the preconnect is allowed by user settings.
  return profile && profile->GetPrefs() &&
         chrome_browser_net::CanPreresolveAndPreconnectUI(profile->GetPrefs());
}

LoadingPredictorConfig::LoadingPredictorConfig()
    : max_navigation_lifetime_seconds(60),
      max_hosts_to_track(100),
      max_origins_per_entry(50),
      max_consecutive_misses(3),
      max_redirect_consecutive_misses(5),
      flush_data_to_disk_delay_seconds(30) {}

LoadingPredictorConfig::LoadingPredictorConfig(
    const LoadingPredictorConfig& other) = default;

LoadingPredictorConfig::~LoadingPredictorConfig() = default;

bool LoadingPredictorConfig::IsSmallDBEnabledForTest() const {
  return max_hosts_to_track == 100;
}

}  // namespace predictors
