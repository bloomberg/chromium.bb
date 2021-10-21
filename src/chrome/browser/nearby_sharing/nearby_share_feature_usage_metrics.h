// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

class PrefService;

// Tracks Nearby Share feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class NearbyShareFeatureUsageMetrics final
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit NearbyShareFeatureUsageMetrics(PrefService* pref_service);
  NearbyShareFeatureUsageMetrics(NearbyShareFeatureUsageMetrics&) = delete;
  NearbyShareFeatureUsageMetrics& operator=(NearbyShareFeatureUsageMetrics&) =
      delete;
  ~NearbyShareFeatureUsageMetrics() override;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;
  void RecordUsage(bool success);

 private:
  PrefService* pref_service_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
