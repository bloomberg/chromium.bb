// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_first_run_field_trials.h"

LocalNtpFirstRunFieldTrialGroup::LocalNtpFirstRunFieldTrialGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage)
    : name_(name), variation_(variation), percentage_(percentage) {}

LocalNtpFirstRunFieldTrialGroup::~LocalNtpFirstRunFieldTrialGroup() = default;

LocalNtpFirstRunFieldTrialConfig::LocalNtpFirstRunFieldTrialConfig(
    const std::string& trial_name)
    : trial_name_(trial_name) {}

LocalNtpFirstRunFieldTrialConfig::~LocalNtpFirstRunFieldTrialConfig() = default;

void LocalNtpFirstRunFieldTrialConfig::AddGroup(
    const std::string& name,
    variations::VariationID variation,
    base::FieldTrial::Probability percentage) {
  groups_.emplace_back(name, variation, percentage);
}
