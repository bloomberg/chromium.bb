// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if defined(OS_MACOSX)
#include "base/enterprise_util.h"
#endif  // defined(OS_MACOSX)

namespace nux {
// This feature flag is used to force the feature to be turned on for non-win
// and non-branded builds, like with tests or development on other platforms.
const base::Feature kNuxOnboardingForceEnabled = {
    "NuxOnboardingForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/onboarding_welcome/welcome_app.js
const base::FeatureParam<std::string> kNuxOnboardingForceEnabledNewUserModules =
    {&kNuxOnboardingForceEnabled, "new-user-modules",
     "nux-google-apps,nux-email,nux-ntp-background,nux-set-as-default,"
     "signin-view"};
const base::FeatureParam<std::string>
    kNuxOnboardingForceEnabledReturningUserModules = {
        &kNuxOnboardingForceEnabled, "returning-user-modules",
        "nux-set-as-default"};
// TODO(hcarmona): remove this flag and all code behind it.
const base::FeatureParam<bool> kNuxOnboardingForceEnabledShowEmailInterstitial =
    {&kNuxOnboardingForceEnabled, "show-email-interstitial", true};

// Our current running experiment of testing the nux-ntp-background module
// depends on the Local NTP feature/experiment being enabled. To avoid polluting
// our data with users who cannot use the nux-ntp-background module, we need
// to check to make sure the Local NTP feature is enabled before running
// any experiment or even reading any feature params from our experiment.
bool CanExperimentWithVariations(Profile* profile) {
  return (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceLocalNtp) ||
          base::FeatureList::IsEnabled(features::kUseGoogleLocalNtp)) &&
         search::DefaultSearchProviderIsGoogle(profile);
}

// Must match study name in configs.
const char kNuxOnboardingStudyName[] = "NaviOnboarding";

std::string GetOnboardingGroup(Profile* profile) {
  if (!CanExperimentWithVariations(profile)) {
    // If we cannot run any variations, we bucket the users into a separate
    // synthetic group that we will ignore data for.
    return "NaviNoVariationSynthetic";
  }

  // We need to use |base::GetFieldTrialParamValue| instead of
  // |base::FeatureParam| because our control group needs a custom value for
  // this param.
  return base::GetFieldTrialParamValue(kNuxOnboardingStudyName,
                                       "onboarding-group");
}

bool IsNuxOnboardingEnabled(Profile* profile) {
  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    return true;
  }

#if defined(GOOGLE_CHROME_BUILD)

#if defined(OS_MACOSX)
  return !base::IsMachineExternallyManaged();
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // To avoid diluting data collection, existing users should not be assigned
  // an onboarding group. So, |prefs::kNaviOnboardGroup| is used to
  // short-circuit the feature checks below.
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  std::string onboard_group = prefs->GetString(prefs::kNaviOnboardGroup);

  if (onboard_group.empty()) {
    // Users who onboarded before Navi or are part of an enterprise.
    return false;
  }

  if (!CanExperimentWithVariations(profile)) {
    return true;  // Default Navi behavior.
  }

  // User will be tied to their original onboarding group, even after
  // experiment ends.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "NaviOnboardingSynthetic", onboard_group);

  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingFeature)) {
    return true;
  }
#endif  // defined(OS_WIN)

#endif  // defined(GOOGLE_CHROME_BUILD)

  return false;
}

base::DictionaryValue GetNuxOnboardingModules(Profile* profile) {
  // This function should not be called when nux onboarding feature is not on.
  DCHECK(nux::IsNuxOnboardingEnabled(profile));

  base::DictionaryValue modules;

  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    modules.SetString("new-user",
                      kNuxOnboardingForceEnabledNewUserModules.Get());
    modules.SetString("returning-user",
                      kNuxOnboardingForceEnabledReturningUserModules.Get());
    modules.SetBoolean("show-email-interstitial",
                       kNuxOnboardingForceEnabledShowEmailInterstitial.Get());
  } else if (CanExperimentWithVariations(profile)) {
    modules.SetString("new-user", kNuxOnboardingNewUserModules.Get());
    modules.SetString("returning-user",
                      kNuxOnboardingReturningUserModules.Get());
    modules.SetBoolean("show-email-interstitial",
                       kNuxOnboardingShowEmailInterstitial.Get());
  } else {
    // Default behavior w/o checking feature flag.
    modules.SetString("new-user", kDefaultNewUserModules);
    modules.SetString("returning-user", kDefaultReturningUserModules);
    modules.SetBoolean("show-email-interstitial", false);
  }

  return modules;
}
}  // namespace nux
