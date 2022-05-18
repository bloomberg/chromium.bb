// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_PREFS_H_
#define CHROME_BROWSER_LACROS_LACROS_PREFS_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace lacros_prefs {

// A preference for whether the "this is an experimental feature" banner has
// been shown to the user.
extern const char kShowedExperimentalBannerPref[];

// Boolean which indicates whether the user finished the Lacros First Run
// Experience.
extern const char kPrimaryProfileFirstRunFinished[];

// Local state prefs are also known as browser-wide prefs. This function
// registers Lacros-related local state prefs.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// This function registers Lacros-related profile specific prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Registers prefs used by extension-controlled prefs. In lacros, these prefs
// hold the computed value across all extensions, which is then sent to ash.
void RegisterExtensionControlledAshPrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace lacros_prefs

#endif  // CHROME_BROWSER_LACROS_LACROS_PREFS_H_
