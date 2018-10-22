// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_first_run_field_trials.h"

// FirstRunFieldTrialGroup
FirstRunFieldTrialGroup::FirstRunFieldTrialGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage)
    : name_(name), variation_(variation), percentage_(percentage) {}

FirstRunFieldTrialGroup::~FirstRunFieldTrialGroup() {}

// FirstRunFieldTrialConfig
FirstRunFieldTrialConfig::FirstRunFieldTrialConfig(
    const std::string& trial_name)
    : FirstRunFieldTrialConfig(trial_name,
                               base::FieldTrialList::kNoExpirationYear,
                               1,
                               1) {}

FirstRunFieldTrialConfig::FirstRunFieldTrialConfig(
    const std::string& trial_name,
    int year,
    int month,
    int day_of_month)
    : trial_name_(trial_name),
      expire_year_(year),
      expire_month_(month),
      expire_day_of_month_(day_of_month) {}

FirstRunFieldTrialConfig::~FirstRunFieldTrialConfig() {}

void FirstRunFieldTrialConfig::AddGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage) {
  groups_.emplace_back(name, variation, percentage);
}
