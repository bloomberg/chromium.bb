// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_field_trials.h"

#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_paths.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_field_trial.h"

void IOSChromeFieldTrials::SetupFieldTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistograms(user_data_dir);
  }
}

void IOSChromeFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Add code here to enable field trials that are active at first run.
  // See http://crrev/c/1128269 for an example.
  location_permissions_field_trial::Create(
      low_entropy_provider, feature_list,
      GetApplicationContext()->GetLocalState());
}
