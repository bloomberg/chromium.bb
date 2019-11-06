// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIALS_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIALS_H_

#include "base/macros.h"
#include "components/variations/variations_associated_data.h"

// Defines a FieldTrial group.
class LocalNtpFirstRunFieldTrialGroup {
 public:
  LocalNtpFirstRunFieldTrialGroup(const std::string& name,
                                  variations::VariationID variation,
                                  base::FieldTrial::Probability percentage);
  ~LocalNtpFirstRunFieldTrialGroup();

  // Accessors for this FieldTrial group.
  const std::string& name() const { return name_; }
  variations::VariationID variation() const { return variation_; }
  base::FieldTrial::Probability percentage() const { return percentage_; }

 private:
  std::string name_;
  variations::VariationID variation_;
  base::FieldTrial::Probability percentage_;
};

// Defines a FieldTrial configuration that will be valid at the time of Chrome
// First Run. Since server-provided FieldTrial configurations need to be
// downloaded, they do not take effect until at least the second run.
// FieldTrials involving substantial UI changes that need to take effect at
// first run must be pre-defined in client code.
class LocalNtpFirstRunFieldTrialConfig {
 public:
  // Initializes with |trial_name| as the name of the FieldTrial with default
  // duration.
  explicit LocalNtpFirstRunFieldTrialConfig(const std::string& trial_name);
  ~LocalNtpFirstRunFieldTrialConfig();

  // Adds a new FieldTrial group of |name| with a probability of |percentage|.
  // |variation| defines a server-side variation configuration.
  void AddGroup(const std::string& name,
                variations::VariationID variation,
                base::FieldTrial::Probability percentage);

  // Returns a vector of FieldTrial groups for this FieldTrial configuration.
  const std::vector<LocalNtpFirstRunFieldTrialGroup>& groups() const {
    return groups_;
  }
  // Accessors for this FieldTrial.
  const std::string& trial_name() { return trial_name_; }

 private:
  std::string trial_name_;
  std::vector<LocalNtpFirstRunFieldTrialGroup> groups_;

  DISALLOW_COPY_AND_ASSIGN(LocalNtpFirstRunFieldTrialConfig);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIALS_H_
