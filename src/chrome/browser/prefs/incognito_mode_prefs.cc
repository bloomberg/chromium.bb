// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if defined(OS_WIN)
#include "chrome/browser/win/parental_controls.h"
#endif  // OS_WIN

#if defined(OS_ANDROID)
#include "chrome/browser/android/partner_browser_customizations.h"
#endif  // defined(OS_ANDROID)

// static
// Sadly, this is required until c++17.
constexpr IncognitoModePrefs::Availability
    IncognitoModePrefs::kDefaultAvailability;

// static
bool IncognitoModePrefs::IntToAvailability(int in_value,
                                           Availability* out_value) {
  if (in_value < 0 || in_value >= AVAILABILITY_NUM_TYPES) {
    *out_value = kDefaultAvailability;
    return false;
  }
  *out_value = static_cast<Availability>(in_value);
  return true;
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  return GetAvailabilityInternal(pref_service, CHECK_PARENTAL_CONTROLS);
}

// static
void IncognitoModePrefs::SetAvailability(PrefService* prefs,
                                         const Availability availability) {
  prefs->SetInteger(prefs::kIncognitoModeAvailability, availability);
}

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kIncognitoModeAvailability,
                                kDefaultAvailability);
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  // Note: This code only checks parental controls if the user requested
  // to launch in incognito mode or if it was forced via prefs. This way,
  // the parental controls check (which can be quite slow) can be avoided
  // most of the time.
  const bool should_use_incognito =
      command_line.HasSwitch(switches::kIncognito) ||
      GetAvailabilityInternal(prefs, DONT_CHECK_PARENTAL_CONTROLS) ==
          IncognitoModePrefs::FORCED;
  return should_use_incognito &&
         GetAvailabilityInternal(prefs, CHECK_PARENTAL_CONTROLS) !=
             IncognitoModePrefs::DISABLED;
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  if (profile->IsGuestSession())
    return true;

  switch (GetAvailability(profile->GetPrefs())) {
    case IncognitoModePrefs::ENABLED:
      return true;

    case IncognitoModePrefs::DISABLED:
      return !profile->IsOffTheRecord();

    case IncognitoModePrefs::FORCED:
      return profile->IsOffTheRecord();

    default:
      NOTREACHED();
      return false;
  }
}

// static
bool IncognitoModePrefs::ArePlatformParentalControlsEnabled() {
#if defined(OS_WIN)
  return GetWinParentalControls().logging_required;
#elif defined(OS_ANDROID)
  return chrome::android::PartnerBrowserCustomizations::IsIncognitoDisabled();
#else
  return false;
#endif
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailabilityInternal(
    const PrefService* pref_service,
    GetAvailabilityMode mode) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(prefs::kIncognitoModeAvailability);
  Availability result = kDefaultAvailability;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
  if (result != IncognitoModePrefs::DISABLED &&
      mode == CHECK_PARENTAL_CONTROLS && ArePlatformParentalControlsEnabled()) {
    if (result == IncognitoModePrefs::FORCED)
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    return IncognitoModePrefs::DISABLED;
  }
  return result;
}
