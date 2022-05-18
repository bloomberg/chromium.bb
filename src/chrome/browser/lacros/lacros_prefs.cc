// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace lacros_prefs {

const char kShowedExperimentalBannerPref[] =
    "lacros.showed_experimental_banner";

const char kPrimaryProfileFirstRunFinished[] =
    "lacros.primary_profile_first_run_finished";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShowedExperimentalBannerPref,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(kPrimaryProfileFirstRunFinished,
                                /*default_value=*/false);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Ordinarily, this preference is registered by Ash, but it is used by
  // browser settings. It could reasonably move to a browser-specific
  // location with suitable #ifdefs.
  registry->RegisterBooleanPref(::prefs::kSettingsShowOSBanner, true);

  // The following two prefs are used in imageWriterPrivate extension api
  // implementation code, which are supported in Lacros.
  registry->RegisterBooleanPref(::prefs::kExternalStorageDisabled, false);
  registry->RegisterBooleanPref(::prefs::kExternalStorageReadOnly, false);
}

void RegisterExtensionControlledAshPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // These settings are used by extensions when the feature itself is controlled
  // by a pref in ash. In lacros, these prefs hold the computed value across all
  // extensions (and also the value set by each individual extension in lacros),
  // and the final value is sent to ash.
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityFocusHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosDockedMagnifierEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityAutoclickEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityCaretHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityCursorColorEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityCursorHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityDictationEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityHighContrastEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityLargeCursorEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityScreenMagnifierEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySelectToSpeakEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySpokenFeedbackEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityStickyKeysEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySwitchAccessEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityVirtualKeyboardEnabled, false);
}

}  // namespace lacros_prefs
