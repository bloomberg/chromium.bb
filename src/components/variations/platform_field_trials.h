// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
#define COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_

#include "base/component_export.h"
#include "base/metrics/field_trial.h"

namespace variations {

// Infrastructure for setting up platform specific field trials. Chrome and
// WebView make use through their corresponding subclasses.
class COMPONENT_EXPORT(VARIATIONS) PlatformFieldTrials {
 public:
  PlatformFieldTrials() = default;

  PlatformFieldTrials(const PlatformFieldTrials&) = delete;
  PlatformFieldTrials& operator=(const PlatformFieldTrials&) = delete;

  virtual ~PlatformFieldTrials() = default;

  // Set up field trials for a specific platform.
  virtual void SetUpFieldTrials() = 0;

  // Create field trials that will control feature list features. This should be
  // called during the same timing window as
  // FeatureList::AssociateReportingFieldTrial. |has_seed| indicates that the
  // variations service used a seed to create field trials. This can be used to
  // prevent associating a field trial with a feature that you expect to be
  // controlled by the variations seed. |low_entropy_provider| can be used as a
  // parameter to creating a FieldTrial that should be visible to Google web
  // properties.
  virtual void SetUpFeatureControllingFieldTrials(
      bool has_seed,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list) = 0;

  // Register any synthetic field trials. Will be called later than the above
  // methods, in particular after g_browser_process is available..
  virtual void RegisterSyntheticTrials() {}
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PLATFORM_FIELD_TRIALS_H_
