// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_FIRST_RUN_FIELD_TRIALS_H_
#define IOS_CHROME_BROWSER_IOS_FIRST_RUN_FIELD_TRIALS_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

// Helper class defining a FieldTrial group.
class FirstRunFieldTrialGroup {
 public:
  FirstRunFieldTrialGroup(const std::string& name,
                          variations::VariationID variation,
                          base::FieldTrial::Probability percentage);
  ~FirstRunFieldTrialGroup();

  // Accessors for this FieldTrial group.
  const std::string& name() const { return name_; }
  variations::VariationID variation() const { return variation_; }
  base::FieldTrial::Probability percentage() const { return percentage_; }

 private:
  std::string name_;
  variations::VariationID variation_;
  base::FieldTrial::Probability percentage_;
};

// Helper class defining a FieldTrial configuration that will be valid at the
// time of Chrome First Run. Since server-provided FieldTrial configurations
// that need to be downloaded will not take effect until at least the second
// run, FieldTrials involving substantial UI changes that need to take effect
// at first run must be pre-defined in client code.
class FirstRunFieldTrialConfig {
 public:
  // Initializes with |trial_name| as the name of the FieldTrial with default
  // duration.
  FirstRunFieldTrialConfig(const std::string& trial_name);
  // Initializes with |trial_name| as the name of the FieldTrial and one that
  // ends on (year, month, day_of_month).
  FirstRunFieldTrialConfig(const std::string& trial_name,
                           int year,
                           int month,
                           int day_of_month);
  ~FirstRunFieldTrialConfig();

  // Adds a new FieldTrial group of |name| with a probability of |percentage|.
  // |variation| defines a server-side variation configuration.
  void AddGroup(const std::string& name,
                variations::VariationID variation,
                base::FieldTrial::Probability percentage);

  // Returns a vector of FieldTrial groups for this FieldTrial configuration.
  const std::vector<FirstRunFieldTrialGroup>& groups() const { return groups_; }
  // Accessors for this FieldTrial.
  const std::string& trial_name() { return trial_name_; }
  int expire_year() { return expire_year_; }
  int expire_month() { return expire_month_; }
  int expire_day_of_month() { return expire_day_of_month_; }

 private:
  std::string trial_name_;
  std::vector<FirstRunFieldTrialGroup> groups_;
  int expire_year_;
  int expire_month_;
  int expire_day_of_month_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunFieldTrialConfig);
};

#endif  // IOS_CHROME_BROWSER_IOS_FIRST_RUN_FIELD_TRIALS_H_
