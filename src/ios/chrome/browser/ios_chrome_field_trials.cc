// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_field_trials.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

namespace {

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Each field trial has 3 trial groups.
// Default - Majority of users will be in default group which receive the
//    default UI. In the case of UI Refresh rollout, this will be the new UI.
// Old UI - A small percentage of users will be held back in the old UI for
//    so usage metrics differences between users with old UI can be compared
//    against users with new UI.
// New UI - Same sized group as Old UI for comparison.
const char kDefaultGroup[] = "NewUIRollout";
const char kOldUIControlGroup[] = "OldUIControl";
const char kNewUIExperimentGroup[] = "NewUIExperiment";
// Experiment IDs defined for the above field trial groups.
const variations::VariationID kDefaultTrialID = 3314527;
const variations::VariationID kOldUIControlTrialID = 3314528;
const variations::VariationID kNewUIExperimentTrialID = 3314529;

// Returns the percentage of users who are to be held back in "old" UI. The
// same percentage will be placed in a separate experiment group, but shown
// the "new" UI such that there will be two equal sized groups for comparing
// user behavior under "old" and "new" UI. Field trial configuration depends
// on release channel.
base::FieldTrial::Probability UIRefreshHoldbackPercent(
    version_info::Channel channel) {
  // There will be no holdback for Old UI starting with M70 release.
  return 0;
}

// Field trials do not take effect until configurations have been downloaded.
// In order for field trials involving substantial UI changes to take effect
// at first run, the configuration must be pre-defined in client code. This
// sets up the field trial for UI Refresh.
void SetupUIRefreshFieldTrial(base::FeatureList* feature_list) {
  // Field trial configuration pre-defined here for UI Refresh should match the
  // configuration stored on the servers. Server-stored configuration will
  // override this pre-defined configuration.
  base::FieldTrial::Probability holdback =
      UIRefreshHoldbackPercent(GetChannel());
  FirstRunFieldTrialConfig config("IOSUIRefreshRolloutWithHoldback");
  config.AddGroup(kDefaultGroup, kDefaultTrialID,
                  kTotalProbability - 2 * holdback);
  config.AddGroup(kOldUIControlGroup, kOldUIControlTrialID, holdback);
  config.AddGroup(kNewUIExperimentGroup, kNewUIExperimentTrialID, holdback);

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          config.trial_name(), kTotalProbability, kDefaultGroup,
          config.expire_year(), config.expire_month(),
          config.expire_day_of_month(), base::FieldTrial::ONE_TIME_RANDOMIZED,
          /*default_group_number=*/nullptr));
  for (const auto& group : config.groups()) {
    variations::AssociateGoogleVariationID(variations::GOOGLE_WEB_PROPERTIES,
                                           config.trial_name(), group.name(),
                                           group.variation());
    trial->AppendGroup(group.name(), group.percentage());
  }

  // Tests which group this user has been assigned to. Based on the group,
  // sets the UI Refresh Phase 1 feature flag.
  const std::string& chosen_group_name = trial->GetGroupNameWithoutActivation();
  base::FeatureList::OverrideState feature_state =
      chosen_group_name == kOldUIControlGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(kUIRefreshPhase1.name, feature_state,
                                           trial.get());
}

}  // namespace

void IOSChromeFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    base::FeatureList* feature_list) {
  SetupUIRefreshFieldTrial(feature_list);
}
