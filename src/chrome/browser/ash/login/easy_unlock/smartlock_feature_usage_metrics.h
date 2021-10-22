// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_

#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"

namespace chromeos {
namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup
}  // namespace chromeos

namespace ash {

// Tracks Smart Lock feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class SmartLockFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate,
      public SmartLockMetricsRecorder::UsageRecorder {
 public:
  explicit SmartLockFeatureUsageMetrics(
      chromeos::multidevice_setup::MultiDeviceSetupClient*
          multi_device_setup_client);
  SmartLockFeatureUsageMetrics(SmartLockFeatureUsageMetrics&) = delete;
  SmartLockFeatureUsageMetrics& operator=(SmartLockFeatureUsageMetrics&) =
      delete;
  ~SmartLockFeatureUsageMetrics() override;

  // SmartLockMetricsRecorder::UsageRecorder:
  void RecordUsage(bool success) override;

 private:
  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;

  chromeos::multidevice_setup::MultiDeviceSetupClient*
      multi_device_setup_client_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_
